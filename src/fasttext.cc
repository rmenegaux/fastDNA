/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "fasttext.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <numeric>


namespace fasttext {

constexpr int32_t FASTTEXT_VERSION = 12; /* Version 1b */
constexpr int32_t FASTTEXT_FILEFORMAT_MAGIC_INT32 = 793712314;

FastText::FastText() : quant_(false) {}

void FastText::addInputVector(Vector& vec, index ind) const {
  if (quant_) {
    vec.addRow(*qinput_, ind);
  } else {
    vec.addRow(*input_, ind);
  }
}

std::shared_ptr<const Dictionary> FastText::getDictionary() const {
  return dict_;
}

const Args FastText::getArgs() const {
  return *args_.get();
}

std::shared_ptr<const Matrix> FastText::getInputMatrix() const {
  return input_;
}

std::shared_ptr<const Matrix> FastText::getOutputMatrix() const {
  return output_;
}

index FastText::getWordId(std::string& word) const {
  // FIXME returns -1 but is not allowed to
  if (word.size() != args_->minn) {
    return -1;
  }
  std::vector<index> ngrams;
  dict_->readSequence(word, ngrams);
  return ngrams.empty() ? -1 : ngrams[0];
}

int32_t FastText::getSubwordId(const std::string& word) const {
  int32_t h = dict_->hash(word) % args_->bucket;
  return dict_->nwords() + h;
}

void FastText::getWordVector(Vector& vec, std::string& word) const {
  std::istringstream in(word);
  getWordVector(vec, in);
}

void FastText::getWordVector(Vector& vec, std::istream& in) const {
  std::vector<index> ngrams;
  dict_->getLine(in, ngrams);
  vec.zero();
  for (int i = 0; i < ngrams.size(); i ++) {
    addInputVector(vec, ngrams[i]);
  }
  if (ngrams.size() > 0) {
    vec.mul(1.0 / ngrams.size());
  }
}

void FastText::getWordVector(Vector& vec, const index i) const {
  vec.zero();
  addInputVector(vec, i);
}


void FastText::saveVectors() {
  std::ofstream ofs(args_->output + ".vec");
  if (!ofs.is_open()) {
    throw std::invalid_argument(
        args_->output + ".vec" + " cannot be opened for saving vectors!");
  }
  ofs << dict_->nwords() << " " << args_->dim << std::endl;
  Vector vec(args_->dim);
  for (index i = 0; i < dict_->nwords(); i++) {
    std::string word = dict_->getSequence(i);
    getWordVector(vec, i);
    ofs << word << " " << vec << std::endl;
  }
  ofs.close();
}

void FastText::saveOutput() {
  std::ofstream ofs(args_->output + ".output");
  if (!ofs.is_open()) {
    throw std::invalid_argument(
        args_->output + ".output" + " cannot be opened for saving vectors!");
  }
  if (quant_) {
    throw std::invalid_argument(
        "Option -saveOutput is not supported for quantized models.");
  }
  int32_t n = (args_->model == model_name::sup) ? dict_->nlabels()
                                                : dict_->nwords();
  ofs << n << " " << args_->dim << std::endl;
  Vector vec(args_->dim);
  for (int32_t i = 0; i < n; i++) {
    std::string word = (args_->model == model_name::sup) ? dict_->getLabel(i)
                                                         : dict_->getSequence(i);
    vec.zero();
    vec.addRow(*output_, i);
    ofs << word << " " << vec << std::endl;
  }
  ofs.close();
}

bool FastText::checkModel(std::istream& in) {
  int32_t magic;
  in.read((char*)&(magic), sizeof(int32_t));
  if (magic != FASTTEXT_FILEFORMAT_MAGIC_INT32) {
    return false;
  }
  in.read((char*)&(version), sizeof(int32_t));
  if (version > FASTTEXT_VERSION) {
    return false;
  }
  // std::cerr << magic << " " << version << std::endl;
  return true;
}

void FastText::signModel(std::ostream& out) {
  const int32_t magic = FASTTEXT_FILEFORMAT_MAGIC_INT32;
  const int32_t version = FASTTEXT_VERSION;
  out.write((char*)&(magic), sizeof(int32_t));
  out.write((char*)&(version), sizeof(int32_t));
}

void FastText::saveModel() {
  std::string fn(args_->output);
  if (quant_) {
    fn += ".ftz";
  } else {
    fn += ".bin";
  }
  saveModel(fn);
}

void FastText::saveModel(const std::string path) {
  std::ofstream ofs(path, std::ofstream::binary);
  if (!ofs.is_open()) {
    throw std::invalid_argument(path + " cannot be opened for saving!");
  }
  signModel(ofs);
  args_->save(ofs);
  dict_->save(ofs);

  ofs.write((char*)&(quant_), sizeof(bool));
  if (quant_) {
    qinput_->save(ofs);
  } else {
    input_->save(ofs);
  }

  ofs.write((char*)&(args_->qout), sizeof(bool));
  if (quant_ && args_->qout) {
    qoutput_->save(ofs);
  } else {
    output_->save(ofs);
  }

  ofs.close();
}

void FastText::loadModel(const std::string& filename) {
  std::ifstream ifs(filename, std::ifstream::binary);
  if (!ifs.is_open()) {
    throw std::invalid_argument(filename + " cannot be opened for loading!");
  }
  if (!checkModel(ifs)) {
    throw std::invalid_argument(filename + " has wrong file format!");
  }
  loadModel(ifs);
  ifs.close();
}

void FastText::loadModel(std::istream& in) {
  args_ = std::make_shared<Args>();
  input_ = std::make_shared<Matrix>();
  output_ = std::make_shared<Matrix>();
  qinput_ = std::make_shared<QMatrix>();
  qoutput_ = std::make_shared<QMatrix>();
  // std::cerr << "Loading args" << std::endl;
  args_->load(in);
  if (version == 11 && args_->model == model_name::sup) {
    // backward compatibility: old supervised models do not use char ngrams.
    args_->maxn = 0;
  }
  // std::cerr << "Loading dict" << std::endl;
  dict_ = std::make_shared<Dictionary>(args_, in);

  bool quant_input;
  // std::cerr << "Loading Input" << std::endl;
  in.read((char*) &quant_input, sizeof(bool));
  if (quant_input) {
    quant_ = true;
    qinput_->load(in);
  } else {
    input_->load(in);
  }

  if (!quant_input && dict_->isPruned()) {
    throw std::invalid_argument(
        "Invalid model file.\n"
        "Please download the updated model from www.fasttext.cc.\n"
        "See issue #332 on Github for more information.\n");
  }
  // std::cerr << "Loading Output" << std::endl;
  in.read((char*) &args_->qout, sizeof(bool));
  if (quant_ && args_->qout) {
    qoutput_->load(in);
  } else {
    output_->load(in);
  }

  model_ = std::make_shared<Model>(input_, output_, args_, 0);
  model_->quant_ = quant_;
  model_->setQuantizePointer(qinput_, qoutput_, args_->qout);
  
 // std::cerr << " set counts" << std::endl;
  if (args_->model == model_name::sup) {
    model_->setTargetCounts(dict_->getLabelCounts());
  } else {
    // model_->setTargetCounts(dict_->getCounts(entry_type::word));
  }
}

void FastText::printInfo(real progress, real loss, std::ostream& log_stream) {
  // clock_t might also only be 32bits wide on some systems
  double t = double(clock() - start_) / double(CLOCKS_PER_SEC);
  double lr = args_->lr * (1.0 - progress);
  double wst = 0;
  int64_t eta = 720 * 3600; // Default to one month
  if (progress > 0 && t >= 0) {
    eta = int(t / progress * (1 - progress));
    wst = double(tokenCount_) / t / args_->thread;
  }
  int32_t etah = eta / 3600;
  int32_t etam = (eta % 3600) / 60;
  progress = progress * 100;
  log_stream << std::fixed;
  log_stream << "Progress: ";
  log_stream << std::setprecision(1) << std::setw(5) << progress << "%";
  log_stream << " fragments/sec/thread: " << std::setw(7) << int64_t(wst);
  log_stream << " lr: " << std::setw(9) << std::setprecision(6) << lr;
  log_stream << " loss: " << std::setw(9) << std::setprecision(6) << loss;
  log_stream << " ETA: " << std::setw(3) << etah;
  log_stream << "h" << std::setw(2) << etam << "m";
  log_stream << std::flush;
}

std::vector<int32_t> FastText::selectEmbeddings(int32_t cutoff) const {
  Vector norms(input_->size(0));
  input_->l2NormRow(norms);
  std::vector<int32_t> idx(input_->size(0), 0);
  // std::iota(idx.begin(), idx.end(), 0);
  // auto eosid = dict_->getId(Dictionary::EOS);
  // std::sort(idx.begin(), idx.end(),
  //     [&norms, eosid] (size_t i1, size_t i2) {
  //     return eosid ==i1 || (eosid != i2 && norms[i1] > norms[i2]);
  //     });
  // idx.erase(idx.begin() + cutoff, idx.end());
  return idx;
}

void FastText::quantize(const Args qargs) {
  if (args_->model != model_name::sup) {
    throw std::invalid_argument(
        "For now we only support quantization of supervised models");
  }
  args_->input = qargs.input;
  args_->qout = qargs.qout;
  args_->output = qargs.output;

  if (qargs.cutoff > 0 && qargs.cutoff < input_->size(0)) {
    auto idx = selectEmbeddings(qargs.cutoff);
    dict_->prune(idx);
    std::shared_ptr<Matrix> ninput =
        std::make_shared<Matrix>(idx.size(), args_->dim);
    for (auto i = 0; i < idx.size(); i++) {
      for (auto j = 0; j < args_->dim; j++) {
        ninput->at(i, j) = input_->at(idx[i], j);
      }
    }
    input_ = ninput;
    if (qargs.retrain) {
      args_->epoch = qargs.epoch;
      args_->lr = qargs.lr;
      args_->thread = qargs.thread;
      args_->verbose = qargs.verbose;
      startThreads();
    }
  }

  qinput_ = std::make_shared<QMatrix>(*input_, qargs.dsub, qargs.qnorm);

  if (args_->qout) {
    qoutput_ = std::make_shared<QMatrix>(*output_, 2, qargs.qnorm);
  }

  quant_ = true;
  model_ = std::make_shared<Model>(input_, output_, args_, 0);
  model_->quant_ = quant_;
  model_->setQuantizePointer(qinput_, qoutput_, args_->qout);
  if (args_->model == model_name::sup) {
    model_->setTargetCounts(dict_->getLabelCounts());
  } else {
    // model_->setTargetCounts(dict_->getCounts(entry_type::word));
  }
}

void FastText::supervised(
    Model& model,
    real lr,
    const std::vector<index>& line,
    const std::vector<int32_t>& labels) {
  if (labels.size() == 0 || line.size() == 0) return;
  std::uniform_int_distribution<> uniform(0, labels.size() - 1);
  int32_t i = uniform(model.rng);
  model.update(line, labels[i], lr);
}

void FastText::cbow(Model& model, real lr,
                    const std::vector<index>& line) {
  // std::vector<int32_t> bow;
  // std::uniform_int_distribution<> uniform(1, args_->ws);
  // for (int32_t w = 0; w < line.size(); w++) {
  //   int32_t boundary = uniform(model.rng);
  //   bow.clear();
  //   for (int32_t c = -boundary; c <= boundary; c++) {
  //     if (c != 0 && w + c >= 0 && w + c < line.size()) {
  //       const std::vector<int32_t>& ngrams = dict_->getSubwords(line[w + c]);
  //       bow.insert(bow.end(), ngrams.cbegin(), ngrams.cend());
  //     }
  //   }
  //   model.update(bow, line[w], lr);
  // }
}

void FastText::skipgram(Model& model, real lr,
                        const std::vector<index>& line) {
  // std::uniform_int_distribution<> uniform(1, args_->ws);
  // for (int32_t w = 0; w < line.size(); w++) {
  //   int32_t boundary = uniform(model.rng);
  //   const std::vector<int32_t>& ngrams = dict_->getSubwords(line[w]);
  //   for (int32_t c = -boundary; c <= boundary; c++) {
  //     if (c != 0 && w + c >= 0 && w + c < line.size()) {
  //       model.update(ngrams, line[w + c], lr);
  //     }
  //   }
  // }
}

std::tuple<int64_t, double, double> FastText::test(
    std::istream& in,
    std::istream& labelfile,
    int32_t k,
    real threshold) {
  int32_t nexamples = 0, nlabels = 0, npredictions = 0;
  double precision = 0.0;
  std::vector<index> line;
  std::vector<int32_t> labels;
  // dict_->printDictionary();
  while (in.peek() != EOF) {
    dict_->getLine(in, line);
    dict_->getLabels(labelfile, labels);
    if (labels.size() > 0 && line.size() > 0) {
      std::vector<std::pair<real, int32_t>> modelPredictions;
      model_->predict(line, k, threshold, modelPredictions);
      for (auto it = modelPredictions.cbegin(); it != modelPredictions.cend(); it++) {
        if (std::find(labels.begin(), labels.end(), it->second) != labels.end()) {
          precision += 1.0;
        }
      }
      nexamples++;
      nlabels += labels.size();
      npredictions += modelPredictions.size();
    }
    // else {
    //   // std::cerr << "line " << line.size() << " label " << " pos " << in.tellg() / 215 << std::endl;
    // }
  }
  return std::tuple<int64_t, double, double>(
      nexamples, precision / npredictions, precision / nlabels);
}

std::tuple<int64_t, double, double> FastText::test_paired(
    std::istream& in,
    std::istream& labelfile,
    int32_t k,
    real threshold) {
  int32_t nexamples = 0, nlabels = 0, npredictions = 0;
  double precision = 0.0;
  std::vector<index> line;
  std::vector<index> line2;
  std::vector<int32_t> labels;
  // dict_->printDictionary();
  while (in.peek() != EOF) {
    dict_->getLine(in, line);
    dict_->getLine(in, line2);
    dict_->getLabels(labelfile, labels);
    if (labels.size() > 0 && (line.size() > 0 || line2.size() > 0)) {
      std::vector<std::pair<real, int32_t>> modelPredictions;
      model_->predict_paired(line, line2, k, threshold, modelPredictions);
      for (auto it = modelPredictions.cbegin(); it != modelPredictions.cend(); it++) {
        if (std::find(labels.begin(), labels.end(), it->second) != labels.end()) {
          precision += 1.0;
        }
      }
      nexamples++;
      nlabels += labels.size();
      npredictions += modelPredictions.size();
    }
    // else {
    //   // std::cerr << "line " << line.size() << " label " << " pos " << in.tellg() / 215 << std::endl;
    // }
  }
  return std::tuple<int64_t, double, double>(
      nexamples, precision / npredictions, precision / nlabels);
}

void FastText::predict(
  std::istream& in,
  int32_t k,
  std::vector<std::pair<real,std::string>>& predictions,
  real threshold
) const {
  std::vector<index> words;
  words.reserve(200);
  predictions.clear();
  dict_->getLine(in, words);
  if (words.empty()) return;
  Vector hidden(args_->dim);
  Vector output(dict_->nlabels());
  std::vector<std::pair<real,int32_t>> modelPredictions;
  model_->predict(words, k, threshold, modelPredictions, hidden, output);
  for (auto it = modelPredictions.cbegin(); it != modelPredictions.cend(); it++) {
    predictions.push_back(std::make_pair(it->first, dict_->getLabel(it->second)));
  }
}

void FastText::predict_paired(
  std::istream& in,
  int32_t k,
  std::vector<std::pair<real,std::string>>& predictions,
  real threshold
) const {
  std::vector<index> words;
  std::vector<index> words2;
  // FIXME, guesses that the sequence length is same as training
  words.reserve(args_->length);
  words2.reserve(args_->length);
  predictions.clear();
  dict_->getLine(in, words);
  dict_->getLine(in, words2);
  // DEBUG
  // if (threshold < 0) {
  //   std::cerr << words.size() - args_->length - args_->minn + 1 << std::endl;
  //   // for (auto i = words.begin(); i != words.end(); ++i) {
  //   //      std::cerr << *i << " " ;
  //   //   }
  //   // std::cerr << std::endl;
  //   threshold = 0;
  // }

  if (words.empty() && words2.empty()) return;
  Vector hidden(args_->dim);
  Vector hidden2(args_->dim);
  Vector output(dict_->nlabels());
  std::vector<std::pair<real,int32_t>> modelPredictions;
  model_->predict(words, k, threshold, modelPredictions, hidden, output);
  for (auto it = modelPredictions.cbegin(); it != modelPredictions.cend(); it++) {
    predictions.push_back(std::make_pair(it->first, dict_->getLabel(it->second)));
  }
}

void FastText::predict(
  std::istream& in,
  int32_t k,
  bool paired_end,
  bool print_prob,
  real threshold
) {

  std::vector<std::pair<real,std::string>> predictions;
  while (in.peek() != EOF) {
    predictions.clear();
    if (paired_end) {
      predict_paired(in, k, predictions, threshold);
    }
    else {
      predict(in, k, predictions, threshold);
    }
    if (predictions.empty()) {
      std::cout << std::endl;
      continue;
    }
    for (auto it = predictions.cbegin(); it != predictions.cend(); it++) {
      if (it != predictions.cbegin()) {
        std::cout << " ";
      }
      std::cout << it->second;
      if (print_prob) {
        std::cout << " " << std::exp(it->first);
      }
    }
    std::cout << std::endl;
  }
}

void FastText::ngramVectors(std::string word) {
  // std::vector<int32_t> ngrams;
  // std::vector<std::string> substrings;
  // Vector vec(args_->dim);
  // dict_->getSubwords(word, ngrams, substrings);
  // for (int32_t i = 0; i < ngrams.size(); i++) {
  //   vec.zero();
  //   if (ngrams[i] >= 0) {
  //     if (quant_) {
  //       vec.addRow(*qinput_, ngrams[i]);
  //     } else {
  //       vec.addRow(*input_, ngrams[i]);
  //     }
  //   }
  //   std::cout << substrings[i] << " " << vec << std::endl;
  // }
}

void FastText::precomputeWordVectors(Matrix& wordVectors) {
  // Vector vec(args_->dim);
  // wordVectors.zero();
  // for (int32_t i = 0; i < dict_->nwords(); i++) {
  //   std::string word = dict_->getWord(i);
  //   getWordVector(vec, word);
  //   real norm = vec.norm();
  //   if (norm > 0) {
  //     wordVectors.addRow(vec, i, 1.0 / norm);
  //   }
  // }
}

void FastText::findNN(
    const Matrix& wordVectors,
    const Vector& queryVec,
    int32_t k,
    const std::set<std::string>& banSet,
    std::vector<std::pair<real, std::string>>& results) {
  // results.clear();
  // std::priority_queue<std::pair<real, std::string>> heap;
  // real queryNorm = queryVec.norm();
  // if (std::abs(queryNorm) < 1e-8) {
  //   queryNorm = 1;
  // }
  // Vector vec(args_->dim);
  // for (int32_t i = 0; i < dict_->nwords(); i++) {
  //   std::string word = dict_->getWord(i);
  //   real dp = wordVectors.dotRow(queryVec, i);
  //   heap.push(std::make_pair(dp / queryNorm, word));
  // }
  // int32_t i = 0;
  // while (i < k && heap.size() > 0) {
  //   auto it = banSet.find(heap.top().second);
  //   if (it == banSet.end()) {
  //     results.push_back(std::pair<real, std::string>(heap.top().first, heap.top().second));
  //     i++;
  //   }
  //   heap.pop();
  // }
}

void FastText::analogies(int32_t k) {
  // std::string word;
  // Vector buffer(args_->dim), query(args_->dim);
  // Matrix wordVectors(dict_->nwords(), args_->dim);
  // precomputeWordVectors(wordVectors);
  // std::set<std::string> banSet;
  // std::cout << "Query triplet (A - B + C)? ";
  // std::vector<std::pair<real, std::string>> results;
  // while (true) {
  //   banSet.clear();
  //   query.zero();
  //   std::cin >> word;
  //   banSet.insert(word);
  //   getWordVector(buffer, word);
  //   query.addVector(buffer, 1.0);
  //   std::cin >> word;
  //   banSet.insert(word);
  //   getWordVector(buffer, word);
  //   query.addVector(buffer, -1.0);
  //   std::cin >> word;
  //   banSet.insert(word);
  //   getWordVector(buffer, word);
  //   query.addVector(buffer, 1.0);

  //   findNN(wordVectors, query, k, banSet, results);
  //   for (auto& pair : results) {
  //     std::cout << pair.second << " " << pair.first << std::endl;
  //   }
  //   std::cout << "Query triplet (A - B + C)? ";
  // }
}

void FastText::trainThread(int32_t threadId) {
  std::ifstream ifs(args_->input);
  const int64_t size_ = utils::size(ifs);
  std::streampos pos;

  // std::cerr << "\r trainThread " << std::endl;

  std::mt19937_64 rng(threadId);
  std::uniform_int_distribution<int64_t> uniform(0, size_-1);

  Model model(input_, output_, args_, threadId);
  if (args_->model == model_name::sup) {
    model.setTargetCounts(dict_->getLabelCounts());
  } else {
  }
  // FIXME
  const int64_t ntokens = size_ / args_->length; // dict_->ntokens();
  int64_t localFragmentCount = 0;
  std::vector<index> line;
  std::vector<int32_t> labels;
  int label;
  while (tokenCount_ < args_->epoch * ntokens) {
    real progress = real(tokenCount_) / (args_->epoch * ntokens);
    real lr = args_->lr * (1.0 - progress);
    if (args_->model == model_name::sup) {
      // Generate random position
      pos = uniform(rng);
      // Get that position's label
      label = dict_->labelFromPos(pos);
      // std::cerr << "\rLabel: " << label << std::endl;
      if (label != -1) {
        labels.clear();
        labels.push_back(label);
        // Go to that position
        utils::seek(ifs, pos);
        if (dict_->readSequence(ifs, line, args_->length, true, rng)) {
          localFragmentCount += 1;
          // std::cerr << "\r supervised " << std::endl;
          supervised(model, lr, line, labels);
        }
      }
    } else if (args_->model == model_name::cbow) {
      localFragmentCount += dict_->getLine(ifs, line, model.rng);
      cbow(model, lr, line);
    } else if (args_->model == model_name::sg) {
      localFragmentCount += dict_->getLine(ifs, line, model.rng);
      skipgram(model, lr, line);
    }
    // FIXME watch out for update rate
    if (localFragmentCount > args_->lrUpdateRate) {
      tokenCount_ += localFragmentCount;
      localFragmentCount = 0;
      if (threadId == 0 && args_->verbose > 1)
        loss_ = model.getLoss();
    }
  }
  if (threadId == 0)
    loss_ = model.getLoss();
  ifs.close();
}

void FastText::loadVectors(std::string filename) {
  // std::cerr << "\rLoading pretrained vectors" << std::endl;
  std::ifstream in(filename);
  std::vector<std::string> words;
  std::shared_ptr<Matrix> mat; // temp. matrix for pretrained vectors
  int64_t n, dim;
  if (!in.is_open()) {
    throw std::invalid_argument(filename + " cannot be opened for loading!");
  }
  in >> n >> dim;
  if (dim != args_->dim || n != dict_->nwords()) {
    throw std::invalid_argument(
        "Dimension of pretrained vectors (" + std::to_string(n) + "," + std::to_string(dim) +
        ") does not match dimension (" + std::to_string(dict_->nwords()) + "," + std::to_string(args_->dim) + ")!");
  }
  // mat = std::make_shared<Matrix>(n, dim);
  input_ = std::make_shared<Matrix>(dict_->nwords(), args_->dim);
  std::string word;
  for (size_t i = 0; i < n; i++) {
    word.clear();
    in >> word;
    for (size_t j = 0; j < dim; j++) {
      in >> input_->at(i, j);
    }
  }
  in.close();
  // std::cerr << "\rVectors loaded" << std::endl;
}

void FastText::train(const Args args) {
  args_ = std::make_shared<Args>(args);
  if (args_->loadModel.size() != 0) {
    loadModel(args_->loadModel);
    // FIXME Check args are compatible??
    args_ = std::make_shared<Args>(args);
  } else {
    dict_ = std::make_shared<Dictionary>(args_);
    if (args_->input == "-") {
      // manage expectations
      throw std::invalid_argument("Cannot use stdin for training!");
    }
    std::ifstream ifs(args_->input);
    if (!ifs.is_open()) {
      throw std::invalid_argument(
          args_->input + " cannot be opened for training!");
    }
    std::ifstream labels(args_->labels);
    if (!labels.is_open()) {
      throw std::invalid_argument(
          args_->labels + " cannot be opened for training!");
    }
    // dict_->readFromFile(ifs);
    // dict_->loadLabelMap();
    dict_->readFromFasta(ifs, labels);
    ifs.close();
    if (args_->pretrainedVectors.size() != 0) {
      loadVectors(args_->pretrainedVectors);
    } else {
      input_ = std::make_shared<Matrix>(dict_->nwords()+args_->bucket, args_->dim);
      input_->uniform(1.0 / args_->dim);
    }

    if (args_->model == model_name::sup) {
      output_ = std::make_shared<Matrix>(dict_->nlabels(), args_->dim);
    } else {
      output_ = std::make_shared<Matrix>(dict_->nwords(), args_->dim);
    }
    output_->zero();
  }
  model_ = std::make_shared<Model>(input_, output_, args_, 0);
  if (args_->model == model_name::sup) {
    model_->setTargetCounts(dict_->getLabelCounts());
  } else {
    // model_->setTargetCounts(dict_->getCounts(entry_type::word));
  }
  startThreads();
}

void FastText::startThreads() {
  start_ = clock();
  tokenCount_ = 0;
  loss_ = -1;
  std::vector<std::thread> threads;
  for (int32_t i = 0; i < args_->thread; i++) {
    threads.push_back(std::thread([=]() { trainThread(i); }));
  }
  // FIXME get file size
  std::ifstream ifs(args_->input);
  const int64_t size_ = utils::size(ifs);
  const int64_t ntokens = size_ / args_->length; // dict_->ntokens();
  // Same condition as trainThread
  while (tokenCount_ < args_->epoch * ntokens) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (loss_ >= 0 && args_->verbose > 1) {
      real progress = real(tokenCount_) / (args_->epoch * ntokens);
      std::cerr << "\r";
      printInfo(progress, loss_, std::cerr);
    }
  }
  for (int32_t i = 0; i < args_->thread; i++) {
    threads[i].join();
  }
  if (args_->verbose > 0) {
      std::cerr << "\r";
      printInfo(1.0, loss_, std::cerr);
      std::cerr << std::endl;
  }
}

int FastText::getDimension() const {
    return args_->dim;
}

bool FastText::isQuant() const {
  return quant_;
}

}
