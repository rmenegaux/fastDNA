/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "dictionary.h"

#include <assert.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <stdexcept>
#include <queue>
#include <algorithm>

namespace fasttext {

const char Dictionary::BOS = '>';

Dictionary::Dictionary(std::shared_ptr<Args> args) : args_(args),
  nlabels_(0), nsequences_(0), pruneidx_size_(-1) {}

Dictionary::Dictionary(std::shared_ptr<Args> args, std::istream& in) : args_(args),
  nsequences_(0), nlabels_(0), pruneidx_size_(-1) {
  load(in);
}

// Add sequence to the dictionary
void Dictionary::add(entry e) {
  nsequences_++;
  // // Find label
  // e.label = findLabel(e.name);
  addLabel(e.label);
  name2label_[e.name] = e.label;
  sequences_.push_back(e);
}

// Returns label index of cursor position
int Dictionary::labelFromPos(const std::streampos& pos) {
  int i = 0;
  // Check if position is greater than file size
  while (i < nsequences_-1 && pos > sequences_[i+1].name_pos) {
    i++;
  }
  if (pos < sequences_[i].seq_pos) {
    return -1; // Position is in the sequence name
  }
  // std::cerr << "\rPos: " << pos << std::endl;
  // std::cerr << "\rSeq: " << i << std::endl;
  // std::cerr << "\rLabel: " << sequences_[i].label << std::endl;
  // std::cerr << "\rIndex: " << label2int_[sequences_[i].label] << std::endl;
  int32_t index = label2int_[sequences_[i].label];
  counts_[index] += 1; 
  return index;
}

void Dictionary::addLabel(const std::string& label) {
  auto it = label2int_.find(label);
  if (it == label2int_.end()) {
    label2int_[label] = nlabels_++;
    counts_.push_back(0);
  }
}

index Dictionary::nwords() const {
  // FIXME
  return 1 << 2*args_->minn;
}

int32_t Dictionary::nlabels() const {
  return nlabels_;
}

int64_t Dictionary::ntokens() const {
  return 0; // ntokens_;
}

bool Dictionary::discard(int32_t id, real rand) const {
  assert(id >= 0);
  // assert(id < nwords_);
  if (args_->model == model_name::sup) return false;
  return rand > pdiscard_[id];
}

uint32_t Dictionary::hash(const std::string& str) const {
  uint32_t h = 2166136261;
  for (size_t i = 0; i < str.size(); i++) {
    h = h ^ uint32_t(str[i]);
    h = h * 16777619;
  }
  return h;
}

int8_t Dictionary::base2int(const char c) const {
  // With this convention, the complementary basepair is
  // (base + 2) % 4
  switch(c) {
    case 'A' :
    case 'a' : return 0;
    case 'C' : return 1;
    case 'T' : return 2;
    case 'G' : return 3;
  }
  throw std::invalid_argument("Non-ACGT character in base2int");
}

char Dictionary::int2base(const int c) const {
  switch(c) {
    case 0 : return 'A';
    case 1 : return 'C';
    case 2 : return 'T';
    case 3 : return 'G';
  }
  throw std::invalid_argument("Number greater than 3 in int2base");
}


bool Dictionary::readSequence(std::istream& in,
                              std::vector<index>& ngrams,
                              std::vector<index>& ngrams_comp,
                              const int length,
                              std::mt19937_64& rng) const {
  // If length is -1, read all sequence

  // mask to keep the index values between 0 and 4**k-1
  index mask = nwords() - 1;
  index index = 0, index_comp = 0;
  int c;
  int8_t val, val_comp;  

  const int k = args_->minn;
  ngrams.clear();
  ngrams_comp.clear();

  std::streambuf& sb = *in.rdbuf();

  int32_t noise;
  std::uniform_real_distribution<> uniform(1, 100000);

  int i = 0;
  while (length == -1 || i < length) {
    if (i >= k) {
      ngrams.push_back(index);
      ngrams_comp.push_back(index_comp);
    }
    c = sb.sbumpc();
    if (c == BOS || c == EOF) {
      // Reached end of sequence
      if (c == BOS) {
        sb.sungetc();
      }
      return (i >= k);
    }
    switch(c) {
      case 'A' :
      case 'a' : { val = 0; val_comp = 2; break;}
      case 'C' :
      case 'c' : { val = 1; val_comp = 3; break;}
      case 't' :
      case 'T' : { val = 2; val_comp = 0; break;}
      case 'g' :
      case 'G' : { val = 3; val_comp = 1; break;}
      default : val = -1;
    }
    if (val >= 0) {
      noise = uniform(rng);
      // random mutation
      if (noise <= args_->noise) {
        val = noise % 4;
        val_comp = (val + 2) % 4;
      }
      index *=  4;
      index += val;
      if (i < k) {
        index_comp += val_comp << 2*i;
      }
      else {
        index_comp /= 4;
        index_comp += val_comp << 2*(k-1);
        index &= mask;
        index_comp &= mask;
      }
      i++;
    }
  }
  if (i >= k) {
    ngrams.push_back(index);
    ngrams_comp.push_back(index_comp);
    return true;
  }
  return false;
}

bool Dictionary::readSequence(std::istream& in,
                              std::vector<index>& ngrams,
                              std::vector<index>& ngrams_comp,
                              const int length) const {
  // If length is -1, read all sequence

  // mask to keep the index values between 0 and 4**k-1
  index mask = nwords() - 1;
  index index = 0, index_comp = 0;
  int c;
  int8_t val, val_comp;  

  const int k = args_->minn;
  ngrams.clear();
  ngrams_comp.clear();

  std::streambuf& sb = *in.rdbuf();

  int i = 0;
  while (length == -1 || i < length) {
    if (i >= k) {
      ngrams.push_back(index);
      ngrams_comp.push_back(index_comp);
    }
    c = sb.sbumpc();
    if (c == BOS || c == EOF) {
      // Reached end of sequence
      if (c == BOS) {
        sb.sungetc();
      }
      return (i >= k);
    }
    switch(c) {
      case 'A' :
      case 'a' : { val = 0; val_comp = 2; break;}
      case 'C' :
      case 'c' : { val = 1; val_comp = 3; break;}
      case 't' :
      case 'T' : { val = 2; val_comp = 0; break;}
      case 'g' :
      case 'G' : { val = 3; val_comp = 1; break;}
      default : val = -1;
    }
    if (val >= 0) {
      index *=  4;
      index += val;
      if (i < k) {
        index_comp += val_comp << 2*i;
      }
      else {
        index_comp /= 4;
        index_comp += val_comp << 2*(k-1);
        index &= mask;
        index_comp &= mask;
      }
      i++;
    }
  }
  if (i >= k) {
    ngrams.push_back(index);
    ngrams_comp.push_back(index_comp);
    return true;
  }
  return false;
}

bool Dictionary::readSequence(std::istream& in,
                              std::vector<index>& ngrams,
                              const int length) const {
  // If length is -1, read all sequence

  // mask to keep the index values between 0 and 4**k-1
  index mask = nwords() - 1;
  index index = 0;
  int c;
  int8_t val;  

  const int k = args_->minn;
  ngrams.clear();

  std::streambuf& sb = *in.rdbuf();

  int i = 0;
  while (length == -1 || i < length) {
    if (i >= k) {
      ngrams.push_back(index);
    }
    c = sb.sbumpc();
    if (c == BOS || c == EOF) {
      // Reached end of sequence
      if (c == BOS) {
        sb.sungetc();
      }
      return (i >= k);
    }
    // c = toupper(c);
    switch(c) {
      case 'A' :
      case 'a' : { val = 0; break;}
      case 'C' :
      case 'c' : { val = 1; break;}
      case 't' :
      case 'T' : { val = 2; break;}
      case 'g' :
      case 'G' : { val = 3; break;}
      default : val = -1;
    }
    if (val >= 0) {
      index *=  4;
      index += val;
      if (i >= k) {
        index &= mask;
      }
      i++;
    }
  }
  if (i >= k) {
    ngrams.push_back(index);
    return true;
  }
  return false;
}

bool Dictionary::readSequence(std::string& word,
                            std::vector<index>& ngrams,
                            std::vector<index>& ngrams_comp) const {
  std::istringstream in(word);
  return readSequence(in, ngrams, ngrams_comp, word.size());
}

std::string Dictionary::getSequence(index index) const {
  std::string seq;
  for(int i = 0; i < args_->minn; i++) {
    // FIXME use push_back with other arithmetic?
    seq.insert(seq.begin(), int2base(index % 4));
    index = index / 4;
  }
  return seq;
}

void Dictionary::readFromFasta(std::istream& fasta, std::istream& labels) {
  std::string line, name;
  entry e;
  e.count = 0;
  std::streampos prev_pos = 0;
  while(std::getline(fasta, line).good()){
    if(line.empty() || line[0] == BOS ){ // Identifier marker
      if( !e.name.empty() ){
        add(e);
        if (args_->verbose > 1) {
          std::cerr << "\rRead sequence n" << nsequences_ << ", " << e.name << "      " <<std::flush;
        }
        e.name.clear();
        e.count = 0;
      }
      if( !line.empty() ){
        e.name = line.substr(1);
        // FIXME check if good
        std::getline(labels, e.label);
        e.seq_pos = fasta.tellg();
        e.name_pos = prev_pos;
      }
    } else {
      e.count += line.length();
    }
    prev_pos = fasta.tellg();
  }

  if( !e.name.empty() ){ // Add the last entry
    add(e);
  }

  if (args_->verbose > 0) {
    std::cerr << "\rRead sequence n" << nsequences_ << ", " << e.name << "       " << std::endl;
    std::cerr << "\rNumber of sequences " << nsequences_ << std::endl;
    std::cerr << "\rNumber of labels: " << nlabels() << std::endl;
    std::cerr << "\rNumber of words: " << nwords() << std::endl;
    // FIXME print total length
    // printDictionary();
  }
  // std::vector<int32_t> ngrams, ngrams_comp;
  // in.clear();
  // in.seekg(sequences_[0].seq_pos);
  // readSequence(in, ngrams, ngrams_comp, 20);
  // in.clear();
  // in.seekg(std::streampos(0));
  // std::cerr << "\rTEST: Ground truth" << std::endl;
  // std::getline(in, line);
  // std::getline(in, line);
  // std::cerr << line.substr(0, 20) << std::endl;
  // std::cerr << "\rSequences " << std::endl;
  // std::cerr << getSequence(ngrams[0]) << std::endl;
  // std::cerr << getSequence(ngrams[10]) << std::endl;
  // std::cerr << "\rReverse sequences " << std::endl;
  // std::cerr << getSequence(ngrams_comp[0]) << std::endl;
  // std::cerr << getSequence(ngrams_comp[10]) << std::endl;
  // if (size_ == 0) {
  //   throw std::invalid_argument(
  //       "Empty vocabulary. Try a smaller -minCount value.");
  // }
}

// FUTURE TESTS
// FindLabel
// std::streampos pos(0);
// std::cerr << "\rPosition " << pos << " has label " << findLabel(pos) << std::endl;
// pos = 1037010900;
// std::cerr << "\rPosition " << pos << " has label " << findLabel(pos) << std::endl;
// pos = 1087195199;
// std::cerr << "\rPosition " << pos << " has label " << findLabel(pos) << std::endl;
// pos = 1087195197;
// std::cerr << "\rPosition " << pos << " has label " << findLabel(pos) << std::endl;
// nlabels = length label2int_
// Assert kmers from readSequence are good

void Dictionary::printDictionary() const {
  if (args_->verbose > 1) {
  // for (auto it = sequences_.begin(); it != sequences_.end(); ++it) {
  //   std::cerr << it->name << " " << it->name_pos << " " << it->seq_pos << " length " << it->count << " label " << it->label << " name " << it->name << std::endl;
  // }
  // for (auto it = name2label_.begin(); it != name2label_.end(); ++it) {
  //   std::cerr << it->first << " " << it->second << std::endl;
  // }
  for (auto it = label2int_.begin(); it != label2int_.end(); ++it) {
    std::cerr << it->first << " " << it->second << std::endl;
  }
  }
}

void Dictionary::readFromFile(std::istream& in) {
  // Maybe reimplement this to be compatible with fasttext format
}


void Dictionary::initTableDiscard() {
  // pdiscard_.resize(size_);
  // for (size_t i = 0; i < size_; i++) {
  //   real f = real(words_[i].count) / real(ntokens_);
  //   pdiscard_[i] = std::sqrt(args_->t / f) + args_->t / f;
  // }
}

std::vector<int64_t> Dictionary::getCounts() const {
  // for (auto& w : words_) {
  //   if (w.type == type) counts.push_back(w.count);
  // }
  std::cerr << std::to_string(counts_.size()) << " labels" << std::endl;
  for (int i = 0; i < counts_.size(); i++) {
    std::cerr << std::to_string(i) << " " << std::to_string(counts_[i]) << std::endl;
  }
  return counts_;
}

void Dictionary::reset(std::istream& in) const {
  if (in.eof()) {
    // FIXME use utils::seek
    in.clear();
    in.seekg(std::streampos(0));
  }
}

int32_t Dictionary::getLine(std::istream& in,
                            std::vector<index>& words,
                            std::minstd_rand& rng) const {
  // FIXME
  std::uniform_real_distribution<> uniform(0, 1);
  std::string token;
  std::vector<index> ngrams;
  std::vector<index> ngrams_comp;

  reset(in);
  words.clear();
  readSequence(in, words, ngrams_comp, -1);

  for(int i = 0; i < ngrams.size(); i++) {
    if (!discard(ngrams[i], uniform(rng))) {
      words.push_back(ngrams[i]);
    }
  }
  return ngrams.size();
}

int32_t Dictionary::getLine(std::istream& in,
                            std::vector<index>& ngrams,
                            std::vector<int32_t>& labels) const {
  std::string label;
  std::vector<index> ngrams_comp;

  reset(in);
  std::streampos pos = in.tellg();
  ngrams.clear();
  labels.clear();
  readSequence(in, ngrams, ngrams_comp, -1);
  std::getline(in, label);
  // if (ngrams.empty() || label.size() < 9) {
  //   in.seekg(pos);
  //   std::string line;
  //   std::getline(in, line);
  //   std::cerr << line << " label " << label << std::endl;
  // }
  auto it = label2int_.find(label.substr(9));
  if (it != label2int_.end()) {
    labels.push_back(it->second);
  }
  return 0;
}

int32_t Dictionary::getLine(std::istream& fasta,
                            std::vector<index>& ngrams) const {
  std::string header;
  // std::vector<index> ngrams_comp;

  // std::streampos begin = fasta.tellg();

  if (fasta.peek() == BOS) {
    std::getline(fasta, header);
  }
  ngrams.clear();
  readSequence(fasta, ngrams, -1);
  return 0;
}

int32_t Dictionary::getLine(std::istream& fasta,
                            std::istream& labelfile,
                            std::vector<index>& ngrams,
                            std::vector<int32_t>& labels) const {
  std::string label, header;
  std::vector<index> ngrams_comp;

  if (fasta.peek() == BOS) {
    std::getline(fasta, header);
  }
  ngrams.clear();
  labels.clear();
  readSequence(fasta, ngrams, ngrams_comp, -1);
  std::getline(labelfile, label);
  auto it = label2int_.find(label);
  if (it != label2int_.end()) {
    labels.push_back(it->second);
  }
  return 0;
}

void Dictionary::pushHash(std::vector<int32_t>& hashes, int32_t id) const {
  if (pruneidx_size_ == 0 || id < 0) return;
  if (pruneidx_size_ > 0) {
    if (pruneidx_.count(id)) {
      id = pruneidx_.at(id);
    } else {
      return;
    }
  }
  // hashes.push_back(nwords_ + id);
}

std::string Dictionary::getLabel(int32_t lid) const {
  if (lid < 0 || lid >= nlabels_) {
    throw std::invalid_argument(
        "Label id is out of range [0, " + std::to_string(nlabels_) + "]");
  }
  // Reverse lookup
  for (auto it=label2int_.begin(); it!=label2int_.end(); ++it) {
    if (it->second == lid) {
      return it->first;
    }
  }
  throw std::invalid_argument("Could not find label " + std::to_string(lid));
}

void Dictionary::saveString(std::ostream& out, const std::string& s) const {
  out.write(s.data(), s.size() * sizeof(char));
  out.put(0);
}

void Dictionary::loadString(std::istream& in, std::string& s) const {
  char c;
  s.clear();
  while ((c = in.get()) != 0) {
    s.push_back(c);
  }
}

void Dictionary::save(std::ostream& out) const {
  int32_t name2labelsize_ = name2label_.size();
  out.write((char*) &nsequences_, sizeof(int32_t));
  out.write((char*) &nlabels_, sizeof(int32_t));
  out.write((char*) &name2labelsize_, sizeof(int32_t));
  for (int32_t i = 0; i < nsequences_; i++) {
    entry e = sequences_[i];
    saveString(out, e.label);
    saveString(out, e.name);
    out.write((char*) &(e.count), sizeof(int64_t));
    out.write((char*) &(e.seq_pos), sizeof(std::streampos));
    out.write((char*) &(e.name_pos), sizeof(std::streampos));
  }
  for (const auto pair : name2label_) {
    saveString(out, pair.first);
    saveString(out, pair.second);
  }
  for (const auto pair : label2int_) {
    saveString(out, pair.first);
    out.write((char*) &(pair.second), sizeof(int32_t));
  }
}

void Dictionary::load(std::istream& in) {
  sequences_.clear();
  //FIXME
  int32_t name2labelsize_;
  in.read((char*) &nsequences_, sizeof(int32_t));
  in.read((char*) &nlabels_, sizeof(int32_t));
  in.read((char*) &name2labelsize_, sizeof(int32_t));
  for (int32_t i = 0; i < nsequences_; i++) {
    entry e;
    loadString(in, e.label);
    loadString(in, e.name);
    in.read((char*) &(e.count), sizeof(int64_t));
    in.read((char*) &(e.seq_pos), sizeof(std::streampos));
    in.read((char*) &(e.name_pos), sizeof(std::streampos));
    sequences_.push_back(e);
  }
  // FIXME
  name2label_.clear();
  for (int32_t i = 0; i < name2labelsize_; i++) {
    std::string name;
    std::string label;
    loadString(in, name);
    loadString(in, label);
    name2label_[name] = label;
  }
  label2int_.clear();
  for (int32_t i = 0; i < nlabels_; i++) {
    int32_t index;
    std::string label;
    loadString(in, label);
    in.read((char*) &index, sizeof(int32_t));
    label2int_[label] = index;
  }
  // initTableDiscard();
  // initNgrams();

  // int32_t word2intsize = std::ceil(size_ / 0.7);
  // word2int_.assign(word2intsize, -1);
  // for (int32_t i = 0; i < size_; i++) {
  //   word2int_[find(words_[i].word)] = i;
  // }
}

// void Dictionary::loadLabelMap() {
//   name2label_.clear();
//   if (args_->labels.size() != 0) {
//     std::ifstream ifs(args_->labels);
//     std::string name, label;
//     if (!ifs.is_open()) {
//       throw std::invalid_argument(args_->labels + " cannot be opened for loading!");
//     }
//     while (ifs >> name >> label) {
//       name2label_[name] = label;
//     }
//     ifs.close();
//   }
// }

void Dictionary::prune(std::vector<int32_t>& idx) {
  // std::vector<int32_t> words, ngrams;
  // for (auto it = idx.cbegin(); it != idx.cend(); ++it) {
  //   if (*it < nwords_) {words.push_back(*it);}
  //   else {ngrams.push_back(*it);}
  // }
  // std::sort(words.begin(), words.end());
  // idx = words;

  // if (ngrams.size() != 0) {
  //   int32_t j = 0;
  //   for (const auto ngram : ngrams) {
  //     pruneidx_[ngram - nwords_] = j;
  //     j++;
  //   }
  //   idx.insert(idx.end(), ngrams.begin(), ngrams.end());
  // }
  // pruneidx_size_ = pruneidx_.size();

  // std::fill(word2int_.begin(), word2int_.end(), -1);

  // int32_t j = 0;
  // // for (int32_t i = 0; i < words_.size(); i++) {
  // //   if (getType(i) == entry_type::label || (j < words.size() && words[j] == i)) {
  // //     words_[j] = words_[i];
  // //     word2int_[find(words_[j].word)] = j;
  // //     j++;
  // //   }
  // // }
  // nwords_ = words.size();
  // size_ = nwords_ +  nlabels_;
  // words_.erase(words_.begin() + size_, words_.end());
  // initNgrams();
}

void Dictionary::dump(std::ostream& out) const {
  // out << words_.size() << std::endl;
  // for (auto it : words_) {
  //   std::string entryType = "word";
  //   if (it.type == entry_type::label) {
  //     entryType = "label";
  //   }
  //   out << it.word << " " << it.count << " " << entryType << std::endl;
  // }
}

}
