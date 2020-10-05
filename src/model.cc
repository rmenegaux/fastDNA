/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "model.h"

#include <iostream>
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <stdexcept>

namespace fasttext {

constexpr int64_t SIGMOID_TABLE_SIZE = 512;
constexpr int64_t MAX_SIGMOID = 8;
constexpr int64_t LOG_TABLE_SIZE = 512;

Model::Model(
    std::shared_ptr<Matrix> wi,
    std::shared_ptr<Matrix> wo,
    std::shared_ptr<Args> args,
    int32_t seed)
    : hidden_(args->dim),
      output_(wo->size(0)),
      grad_(args->dim),
      rng(seed),
      quant_(false) {
  wi_ = wi;
  wo_ = wo;
  args_ = args;
  osz_ = wo->size(0);
  hsz_ = args->dim;
  negpos = 0;
  loss_ = 0.0;
  nexamples_ = 1;
  t_sigmoid_.reserve(SIGMOID_TABLE_SIZE + 1);
  t_log_.reserve(LOG_TABLE_SIZE + 1);
  initSigmoid();
  initLog();
}

void Model::setQuantizePointer(std::shared_ptr<QMatrix> qwi,
                               std::shared_ptr<QMatrix> qwo, bool qout) {
  qwi_ = qwi;
  qwo_ = qwo;
  if (qout) {
    osz_ = qwo_->getM();
  }
}

real Model::binaryLogistic(int32_t target, bool label, real lr) {
  real score = sigmoid(wo_->dotRow(hidden_, target));
  real alpha = lr * (real(label) - score);
  grad_.addRow(*wo_, target, alpha);
  wo_->addRow(hidden_, target, alpha);
  if (label) {
    return -log(score);
  } else {
    return -log(1.0 - score);
  }
}

real Model::negativeSampling(int32_t target, real lr) {
  real loss = 0.0;
  grad_.zero();
  for (int32_t n = 0; n <= args_->neg; n++) {
    if (n == 0) {
      loss += binaryLogistic(target, true, lr);
    } else {
      loss += binaryLogistic(getNegative(target), false, lr);
    }
  }
  return loss;
}

real Model::hierarchicalSoftmax(int32_t target, real lr) {
  real loss = 0.0;
  grad_.zero();
  const std::vector<bool>& binaryCode = codes[target];
  const std::vector<int32_t>& pathToRoot = paths[target];
  for (int32_t i = 0; i < pathToRoot.size(); i++) {
    loss += binaryLogistic(pathToRoot[i], binaryCode[i], lr);
  }
  return loss;
}

void Model::computeOutputSoftmax(Vector& hidden, Vector& output) const {
  if (quant_ && args_->qout) {
    output.mul(*qwo_, hidden);
  } else {
    output.mul(*wo_, hidden);
  }
  real max = output[0], z = 0.0;
  for (int32_t i = 0; i < osz_; i++) {
    max = std::max(output[i], max);
  }
  for (int32_t i = 0; i < osz_; i++) {
    output[i] = exp(output[i] - max);
    z += output[i];
  }
  for (int32_t i = 0; i < osz_; i++) {
    output[i] /= z;
  }
}

void Model::computeOutputSoftmax() {
  computeOutputSoftmax(hidden_, output_);
}

real Model::softmax(int32_t target, real lr) {
  grad_.zero();
  computeOutputSoftmax();
  for (int32_t i = 0; i < osz_; i++) {
    real label = (i == target) ? 1.0 : 0.0;
    real alpha = lr * (label - output_[i]);
    grad_.addRow(*wo_, i, alpha);
    wo_->addRow(hidden_, i, alpha);
  }
  return -log(output_[target]);
}

void Model::computeHidden(const std::vector<index>& input, Vector& hidden) const {
  assert(hidden.size() == hsz_);
  hidden.zero();
  for (auto it = input.cbegin(); it != input.cend(); ++it) {
    if(quant_) {
      hidden.addRow(*qwi_, *it);
    } else {
      hidden.addRow(*wi_, *it);
    }
  }
  hidden.mul(1.0 / input.size());
}

bool Model::comparePairs(const std::pair<real, int32_t> &l,
                         const std::pair<real, int32_t> &r) {
  return l.first > r.first;
}

void Model::predict(const std::vector<index>& input, int32_t k, real threshold,
                    std::vector<std::pair<real, int32_t>>& heap,
                    Vector& hidden, Vector& output) const {
  if (k <= 0) {
    throw std::invalid_argument("k needs to be 1 or higher!");
  }
  if (args_->model != model_name::sup) {
    throw std::invalid_argument("Model needs to be supervised for prediction!");
  }
  heap.reserve(k + 1);
  computeHidden(input, hidden);
  if (args_->loss == loss_name::hs) {
    dfs(k, threshold, 2 * osz_ - 2, 0.0, heap, hidden);
  } else {
    computeOutputSoftmax(hidden, output);
    findKBest(k, threshold, heap, output);
  }
  std::sort_heap(heap.begin(), heap.end(), comparePairs);
}

void Model::predict_paired(
  const std::vector<index>& input, const std::vector<index>& input2,
  int32_t k, real threshold,
  std::vector<std::pair<real, int32_t>>& heap,
  Vector& hidden, Vector& hidden2, Vector& output, Vector& output2) const {
  if (k <= 0) {
    throw std::invalid_argument("k needs to be 1 or higher!");
  }
  if (args_->model != model_name::sup) {
    throw std::invalid_argument("Model needs to be supervised for prediction!");
  }
  heap.reserve(k + 1);
  computeHidden(input, hidden);
  computeHidden(input2, hidden2);
  if (args_->loss == loss_name::hs) {
    throw std::invalid_argument("Paired end predictions are not implemented with hierarchical softmax loss.");
    // dfs(k, threshold, 2 * osz_ - 2, 0.0, heap, hidden);
  } else {
    computeOutputSoftmax(hidden, output);
    computeOutputSoftmax(hidden2, output2);
    output.addVector(output2);
    output.mul(0.5);
    findKBest(k, threshold, heap, output);
  }
  std::sort_heap(heap.begin(), heap.end(), comparePairs);
}

void Model::predict(
  const std::vector<index>& input,
  int32_t k,
  real threshold,
  std::vector<std::pair<real, int32_t>>& heap
) {
  predict(input, k, threshold, heap, hidden_, output_);
}

void Model::predict_paired(
  const std::vector<index>& input,
  const std::vector<index>& input2,
  int32_t k,
  real threshold,
  std::vector<std::pair<real, int32_t>>& heap
) {
  Vector hidden2(hsz_);
  Vector output2(osz_);
  predict_paired(input, input2, k, threshold, heap, hidden_, hidden2, output_, output2);
}

void Model::findKBest(
  int32_t k,
  real threshold,
  std::vector<std::pair<real, int32_t>>& heap,
  Vector& output
) const {
  for (int32_t i = 0; i < osz_; i++) {
    if (output[i] < threshold) continue;
    if (heap.size() == k && std_log(output[i]) < heap.front().first) {
      continue;
    }
    heap.push_back(std::make_pair(std_log(output[i]), i));
    std::push_heap(heap.begin(), heap.end(), comparePairs);
    if (heap.size() > k) {
      std::pop_heap(heap.begin(), heap.end(), comparePairs);
      heap.pop_back();
    }
  }
}

void Model::dfs(int32_t k, real threshold, int32_t node, real score,
                std::vector<std::pair<real, int32_t>>& heap,
                Vector& hidden) const {
  if (score < std_log(threshold)) return;
  if (heap.size() == k && score < heap.front().first) {
    return;
  }

  if (tree[node].left == -1 && tree[node].right == -1) {
    heap.push_back(std::make_pair(score, node));
    std::push_heap(heap.begin(), heap.end(), comparePairs);
    if (heap.size() > k) {
      std::pop_heap(heap.begin(), heap.end(), comparePairs);
      heap.pop_back();
    }
    return;
  }

  real f;
  if (quant_ && args_->qout) {
    f= qwo_->dotRow(hidden, node - osz_);
  } else {
    f= wo_->dotRow(hidden, node - osz_);
  }
  f = 1. / (1 + std::exp(-f));

  dfs(k, threshold, tree[node].left, score + std_log(1.0 - f), heap, hidden);
  dfs(k, threshold, tree[node].right, score + std_log(f), heap, hidden);
}

void Model::update(const std::vector<index>& input, int32_t target, real lr) {
  assert(target >= 0);
  assert(target < osz_);
  if (input.size() == 0) return;
  computeHidden(input, hidden_);
  if (args_->loss == loss_name::ns) {
    loss_ += negativeSampling(target, lr);
  } else if (args_->loss == loss_name::hs) {
    loss_ += hierarchicalSoftmax(target, lr);
  } else {
    loss_ += softmax(target, lr);
  }
  nexamples_ += 1;
  
  if (!args_->freezeEmbeddings) {
    if (args_->model == model_name::sup) {
      grad_.mul(1.0 / input.size());
    }
    for (auto it = input.cbegin(); it != input.cend(); ++it) {
      wi_->addRow(grad_, *it, 1.0);
    }
  }
}

void Model::setTargetCounts(const std::vector<int64_t>& counts) {
  assert(counts.size() == osz_);
  if (args_->loss == loss_name::ns) {
    initTableNegatives(counts);
  }
  if (args_->loss == loss_name::hs) {
    buildTree(counts);
  }
}

void Model::initTableNegatives(const std::vector<int64_t>& counts) {
  real z = 0.0;
  for (size_t i = 0; i < counts.size(); i++) {
    z += pow(counts[i], 0.5);
  }
  for (size_t i = 0; i < counts.size(); i++) {
    real c = pow(counts[i], 0.5);
    for (size_t j = 0; j < c * NEGATIVE_TABLE_SIZE / z; j++) {
      negatives_.push_back(i);
    }
  }
  std::shuffle(negatives_.begin(), negatives_.end(), rng);
}

index Model::getNegative(index target) {
  index negative;
  do {
    negative = negatives_[negpos];
    negpos = (negpos + 1) % negatives_.size();
  } while (target == negative);
  return negative;
}

void Model::buildTree(const std::vector<int64_t>& counts) {
  initTree();
  for (int32_t i = 0; i < osz_; i++) {
    tree[i].count = counts[i];
  }
  int32_t leaf = osz_ - 1;
  int32_t node = osz_;
  for (int32_t i = osz_; i < 2 * osz_ - 1; i++) {
    int32_t mini[2];
    for (int32_t j = 0; j < 2; j++) {
      if (leaf >= 0 && tree[leaf].count < tree[node].count) {
        mini[j] = leaf--;
      } else {
        mini[j] = node++;
      }
    }
    tree[i].left = mini[0];
    tree[i].right = mini[1];
    tree[i].count = tree[mini[0]].count + tree[mini[1]].count;
    tree[mini[0]].parent = i;
    tree[mini[1]].parent = i;
    tree[mini[1]].binary = true;
  }
  buildTreePaths();
}

void Model::buildTreePaths() {
  for (int32_t i = 0; i < osz_; i++) {
    std::vector<int32_t> path;
    std::vector<bool> code;
    int32_t j = i;
    while (tree[j].parent != -1) {
      path.push_back(tree[j].parent - osz_);
      code.push_back(tree[j].binary);
      j = tree[j].parent;
    }
    paths.push_back(path);
    codes.push_back(code);
  }
}

void Model::initTree() {
  tree.resize(2 * osz_ - 1);
  for (int32_t i = 0; i < 2 * osz_ - 1; i++) {
    tree[i].parent = -1;
    tree[i].left = -1;
    tree[i].right = -1;
    tree[i].count = 1e15;
    tree[i].binary = false;
  }
}

void Model::loadTreeFromFile(std::istream& in, const std::map<std::string, int>& label2int) {
  initTree();
  char c;
  int64_t count;
  int node_id;
  int parent_node_id;
  std::vector<std::string> taxids;
  std::string line;
  std::string taxid;

  for (size_t i = 0; i < 2 * osz_ - 1; i++) {
    std::getline(in, line);
    std::cerr << line << std::endl; 
    std::istringstream iss(line);
    c = iss.get(); // first char says whether node is leaf
    if (i == 0) {
      iss >> node_id >> count; // root node has no parent
      std::cerr << "node id " << node_id << " count " << count << std::endl;
      for (const auto pair : label2int) {
        std::cerr << pair.first << " -> " << pair.second << std::endl;
      }
    } else {
      iss >> node_id >> parent_node_id >> count;
      std::cerr << "node id " << node_id << "parent id " << parent_node_id << " count " << count << std::endl;
      parent_node_id = 2 * osz_ - 2 - parent_node_id;
    }
    if (c == 'l'){ // leaf
      iss >> taxid;
      node_id = label2int.at(taxid);
    }
    else if (c == 'n'){ // non leaf
      while (iss >> taxid) {
        std::cerr << "read id " << taxid << std::endl;
        taxids.push_back(taxid);
      }
      node_id = 2 * osz_ - 2 - node_id;
    }
    else { 
      std::cerr << "wring first char" << std::endl;
      readTreeError(); 
    }
    tree[node_id].count = count;
    if (i > 0) {
      tree[node_id].parent = parent_node_id;
      if (tree[parent_node_id].left == -1) {
        tree[parent_node_id].left = node_id;
        tree[node_id].binary = true;
      }
      else if (tree[parent_node_id].right == -1) {
        tree[parent_node_id].right = node_id;
        tree[node_id].binary = false;
      }
      else { 
        std::cerr << osz_ << " output size " << std::endl;
        std::cerr << parent_node_id << " has 2 children already goddammit " << std::endl;
        readTreeError();
      }
    }
  }
  buildTreePaths();
}

void Model::readTreeError() {
  throw std::invalid_argument("Invalid format for " + args_->taxonomy);
}

void Model::saveTree(std::ostream& out) {
  for (size_t i = 0; i < 2 * osz_ - 1; i++) {
    if (i < 2 * osz_ - 2) {
      out.write((char*) &tree[i].parent, sizeof(int));
    }
    if (i >= osz_) {
      out.write((char*) &tree[i].left, sizeof(int));
      out.write((char*) &tree[i].right, sizeof(int));
    }
    out.write((char*) &tree[i].count, sizeof(int64_t));
    out.put(0);
  }
}

void Model::loadTree(std::istream& in) {
  initTree();
  for (size_t i = 0; i < 2 * osz_ - 1; i++) {
    if (i < 2 * osz_ - 2) {
      in.read((char*) &(tree[i].parent), sizeof(int));
    }
    if (i >= osz_) {
      in.read((char*) &tree[i].left, sizeof(int));
      tree[tree[i].left].binary = true;
      in.read((char*) &tree[i].right, sizeof(int));
      tree[tree[i].right].binary = false;
    }
    in.read((char*) &(tree[i].count), sizeof(int64_t));
    in.get();
  }
  buildTreePaths();
}

real Model::getLoss() const {
  return loss_ / nexamples_;
}

void Model::initSigmoid() {
  for (int i = 0; i < SIGMOID_TABLE_SIZE + 1; i++) {
    real x = real(i * 2 * MAX_SIGMOID) / SIGMOID_TABLE_SIZE - MAX_SIGMOID;
    t_sigmoid_.push_back(1.0 / (1.0 + std::exp(-x)));
  }
}

void Model::initLog() {
  for (int i = 0; i < LOG_TABLE_SIZE + 1; i++) {
    real x = (real(i) + 1e-5) / LOG_TABLE_SIZE;
    t_log_.push_back(std::log(x));
  }
}

real Model::log(real x) const {
  if (x > 1.0) {
    return 0.0;
  }
  int64_t i = int64_t(x * LOG_TABLE_SIZE);
  return t_log_[i];
}

real Model::std_log(real x) const {
  return std::log(x+1e-5);
}

real Model::sigmoid(real x) const {
  if (x < -MAX_SIGMOID) {
    return 0.0;
  } else if (x > MAX_SIGMOID) {
    return 1.0;
  } else {
    int64_t i = int64_t((x + MAX_SIGMOID) * SIGMOID_TABLE_SIZE / MAX_SIGMOID / 2);
    return t_sigmoid_[i];
  }
}

}
