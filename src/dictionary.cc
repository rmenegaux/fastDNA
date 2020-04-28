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

/*
Indexing schema for k-mers
A k-mer is attributed to one of the 10 subparts
according its first and last bases

--------------------------
0/ nwords(k-2) : A-------T
--------------------------
1/ nwords(k-2) : T-------A
--------------------------
2/ nwords(k-2) : C-------G
--------------------------
3/ nwords(k-2) : G-------C
--------------------------
4/ 4^(k-2) :     A-A & T-T
--------------------------
5/ 4^(k-2) :     C-C & G-G
--------------------------
6/ 4^(k-2) :     A-C & G-T
--------------------------
7/ 4^(k-2) :     C-A & T-G
--------------------------
8/ 4^(k-2) :     A-G & C-T
--------------------------
9/ 4^(k-2) :     G-A & T-C
--------------------------
*/

// translates first and last base to a subpart of the indexing
const std::vector<int8_t> Dictionary::ends2ind_
{  4 , 6 , 8 , 0 , 7 , 5 , 2 , 14 , 9 , 3 , 11 , 12 , 1 , 15 , 13 , 10 };
// AA, AC, AG, AT, CA, CC, CG, CT, GA, GC, GG, GT, TA, TC, TG, TT 

// translates first and last base to a subpart of the indexing
// const std::vector<int8_t> Dictionary::ind2ends_
// {  3 , 12, 6 , 9 , 0 , 5 , 1 , 4 , 2 , 8 };
// // AT, TA, CG, GC, AA, CC, AC, CA, AG, GA
const std::vector<std::pair<char, char>> Dictionary::ind2ends_
{{'A', 'T'}, {'T', 'A'}, {'C', 'G'}, {'G', 'C'},
 {'A', 'A'}, {'C', 'C'}, {'A', 'C'}, {'C', 'A'}, {'A', 'G'}, {'G', 'A'}};

// Add sequence to the dictionary
void Dictionary::add(entry e) {
  nsequences_++;
  // // Find label
  // e.label = findLabel(e.name);
  addLabel(e);
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
  // counts_[index] += 1; 
  return index;
}

void Dictionary::addLabel(const entry e) {
  auto it = label2int_.find(e.label);
  if (it == label2int_.end()) {
    label2int_[e.label] = nlabels_++;
    counts_.push_back(e.count);
  } 
  else {
    counts_[label2int_[e.label]] += e.count;
  }
}

index Dictionary::nwords(const int8_t k) const {
  index nword = 1 << (2*k - 1);
  if (k % 2 == 0) {
    nword += 1 << (k-1);
  }
  return nword;
}

index Dictionary::nwords() const {
  return nwords(args_->minn);
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
    case 2 : return 'G';
    case 3 : return 'T';
  }
  throw std::invalid_argument("Number greater than 3 in int2base");
}


index Dictionary::computeIndex(index kmer,
                               index kmer_reverse,
                               const int k) const {
  if (k == 0) { return 0; }
  if (k == 1) { return kmer % 2; }
  index begin = kmer >> 2*(k - 1); // first 2 bits -> first base
  index end = kmer & 3; // last 2 bits -> last base
  index position = ends2ind_[(begin << 2) + end];
  // erase last base
  kmer = kmer >> 2;
  kmer_reverse = kmer_reverse >> 2;
  // erase first base
  kmer &= (1 << 2*(k-2)) - 1;
  kmer_reverse &= (1 << 2*(k-2)) - 1;

  if (position < 4) {
    return position * nwords(k - 2) + computeIndex(kmer, kmer_reverse, k-2);
  } else if (position < 10) {
    return 4 * nwords(k - 2) + ((position - 4) << 2*(k-2)) + kmer;
  } else {
    return 4 * nwords(k - 2) + ((position - 10) << 2*(k-2)) + kmer_reverse;
  }
}

bool Dictionary::readSequence(std::istream& in,
                              std::vector<index>& ngrams,
                              const int length,
                              bool add_noise,
                              std::mt19937_64& rng) const {
  // If length is -1, read all sequence

  const int8_t k = args_->minn;
  // mask to keep the index values between 0 and 4**k-1
  index mask = (1 << 2*k) - 1;
  index index = 0, index_reverse = 0;
  int c;
  int8_t val, val_reverse;

  ngrams.clear();

  std::streambuf& sb = *in.rdbuf();

  int32_t noise;
  std::uniform_real_distribution<> uniform(1, 100000);

  int i = 0;
  while (length == -1 || i < length) {
    if (i >= k) {
      ngrams.push_back(computeIndex(index, index_reverse, k));
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
      case 'a' : { val = 0; break;}
      case 'C' :
      case 'c' : { val = 1; break;}
      case 'g' :
      case 'G' : { val = 2; break;}
      case 't' :
      case 'T' : { val = 3; break;}
      default : val = -1;
    }
    if (val >= 0) {
      if (add_noise) {
        noise = uniform(rng);
        // random mutation
        if (noise <= args_->noise) {
          val = noise % 4;
        }
      }
      val_reverse = 3 - val;

      index *=  4;
      index += val;
      if (i < k) {
        index_reverse += val_reverse << 2*i;
      }
      else {
        index_reverse /= 4;
        index_reverse += val_reverse << 2*(k-1);
        index &= mask;
        index_reverse &= mask;
      }
      i++;
    }
  }
  if (i >= k) {
    ngrams.push_back(computeIndex(index, index_reverse, k));
    return true;
  }
  return false;
}

bool Dictionary::readSequence(std::istream& in,
                              std::vector<index>& ngrams,
                              const int length) const {
  // If length is -1, read all sequence
  std::mt19937_64 dummy(0);
  return readSequence(in, ngrams, length, false, dummy);
}

bool Dictionary::readSequence(std::string& word,
                            std::vector<index>& ngrams) const {
  std::istringstream in(word);
  return readSequence(in, ngrams, word.size());
}

std::string Dictionary::getSequence(index ind) const {
  // Returns the first k-mer in lexicographical order from the pair of possible k-mers
  std::string seq;
  getSequenceRCI(seq, ind, args_->minn);
  // std::cerr << ind << ": " << seq << std::endl;
  return seq; // getSequenceRCI(ind, args_->minn);
}

void Dictionary::getSequenceRCI(std::string& seq, index ind, const int8_t k) const {

  if (k == 0) { return; }
  if (k == 1) { 
    seq.push_back(int2base(ind % 2));
    return;
  }
  int position;
  if (k == 2) {
    position = ind;
  }
  else { 
    int m = nwords(k-2);
    // std::cerr << "nwords(k-2) " << m << std::endl;
    position = ind / m;
    // std::cerr << "position " << position << std::endl;
    if (position < 4) {
      getSequenceRCI(seq, ind % m, k-2);
    }
    else {
      ind -= 4*m;
      // std::cerr << "index " << ind << std::endl;
      position = 4 + ind / (1 << 2*(k-2));
      ind = ind % (1 << 2*(k-2));
      // std::cerr << "new position " << position << std::endl;
      getSequenceClassic(seq, ind, k-2);
    }
  }
  // std::cerr << seq << std::endl;
  std::pair<char, char> first_last = ind2ends_[position];
  seq.insert(seq.begin(), first_last.first);
  seq.push_back(first_last.second);
}

void Dictionary::getSequenceClassic(std::string& seq, index ind, const int8_t k) const {
  for(int i = 0; i < k; i++) {
    seq.insert(seq.begin(), int2base(ind % 4));
    ind /= 4;
  }
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
    std::cerr << "\rNumber of sequences: " << nsequences_ << std::endl;
    std::cerr << "\rNumber of labels: " << nlabels() << std::endl;
    std::cerr << "\rNumber of words: " << nwords() << std::endl;
    // FIXME print total length
    // printDictionary();
  }
  // std::vector<index> ngrams;
  // fasta.clear();
  // fasta.seekg(sequences_[0].seq_pos);
  // readSequence(fasta, ngrams, 20);
  // fasta.clear();
  // fasta.seekg(std::streampos(0));
  // std::cerr << "\rTEST: Ground truth" << std::endl;
  // std::getline(fasta, line);
  // std::getline(fasta, line);
  // std::cerr << line.substr(0, 20) << std::endl;
  // std::cerr << "\rSequences " << std::endl;
  // std::cerr << getSequence(ngrams[0]) << std::endl;
  // std::cerr << getSequence(ngrams[10]) << std::endl;
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

std::vector<int64_t> Dictionary::getLabelCounts() const {
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

  reset(in);
  words.clear();
  readSequence(in, words, -1);

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

  reset(in);
  std::streampos pos = in.tellg();
  ngrams.clear();
  labels.clear();
  readSequence(in, ngrams, -1);
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

  if (fasta.peek() == BOS) {
    std::getline(fasta, header);
  }
  ngrams.clear();
  readSequence(fasta, ngrams, -1);
  return 0;
}

int32_t Dictionary::getLabels(std::istream& labelfile,
                              std::vector<int32_t>& labels) const {
  std::string label;
  labels.clear();
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
  counts_.clear();
  for (int32_t i = 0; i < nlabels_; i++) {
    int32_t index;
    std::string label;
    loadString(in, label);
    in.read((char*) &index, sizeof(int32_t));
    label2int_[label] = index;
    counts_.push_back(0);
  }
  // Recount labels
  // Maybe better to save and load counts
  for (auto it = sequences_.cbegin(); it != sequences_.cend(); ++it) {
    addLabel(*it);
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
