/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <vector>
#include <string>
#include <istream>
#include <ostream>
#include <random>
#include <memory>
#include <unordered_map>
#include <map>

#include "args.h"
#include "real.h"

namespace fasttext {

typedef int32_t id_type;

// struct entry {
//   std::string word;
//   int64_t count;
//   entry_type type;
//   std::vector<int32_t> subwords;
// };

struct entry {
  std::string name;
  std::string label;
  std::streampos name_pos;
  std::streampos seq_pos;
  int64_t count;
};

class Dictionary {
  protected:
    static const int32_t MAX_VOCAB_SIZE = 30000000;
    static const int32_t MAX_LINE_SIZE = 1024;
    static const std::vector<int8_t> ends2ind_;
    static const std::vector<std::pair<char, char>> ind2ends_;

    void reset(std::istream&) const;
    void pushHash(std::vector<int32_t>& hashes, int32_t id) const;
    std::shared_ptr<Args> args_;
    std::vector<entry> sequences_;
    std::map<std::string, std::string> name2label_;
    std::map<std::string, int> label2int_;

    std::vector<real> pdiscard_;
    int32_t nlabels_;
    int32_t nsequences_;
    int64_t pruneidx_size_;
    std::vector<int64_t> counts_;
    std::unordered_map<int32_t, int32_t> pruneidx_;

  public:
    static const char BOS;

    explicit Dictionary(std::shared_ptr<Args>);
    explicit Dictionary(std::shared_ptr<Args>, std::istream&);
    void addLabel(const entry);
    index nwords(const int8_t k) const;
    index nwords() const;
    int32_t nlabels() const;
    int64_t ntokens() const;
    bool discard(int32_t, real) const;
    uint32_t hash(const std::string& str) const;
    void add(const entry);
    std::string findLabel(const std::string&);
    int labelFromPos(const std::streampos&);
    void readFromFasta(std::istream& fasta, std::istream& labels);
    void printDictionary() const;
    void readFromFile(std::istream& in);
    void initTableDiscard(); 
    std::string getLabel(int32_t) const;
    void saveString(std::ostream& out, const std::string& s) const;
    void loadString(std::istream& in, std::string& s) const;
    void save(std::ostream&) const;
    void load(std::istream&);
    // void loadLabelMap();
    std::vector<int64_t> getLabelCounts() const;
    int32_t getLine(std::istream&, std::vector<index>&, std::vector<int32_t>&)
        const;
    int32_t getLine(std::istream&, std::vector<index>&,
                    std::minstd_rand&) const;
    int32_t getLine(std::istream& fasta,
                            std::vector<index>& ngrams) const;
    int32_t getLabels(std::istream& labelfile,
                                  std::vector<int32_t>& labels) const;
    void prune(std::vector<int32_t>&);
    bool isPruned() { return pruneidx_size_ >= 0; }
    void dump(std::ostream&) const;
    int8_t base2int(const char c) const;
    char int2base(const int c) const;
    index computeIndex(index ind,
                       index index_reverse,
                       const int k) const;
    std::string getSequence(index i) const;
    void getSequenceRCI(std::string& seq, index index, const int8_t k) const;
    void getSequenceClassic(std::string& seq, index index, const int8_t k) const;
    bool readSequence(
        std::istream& in, std::vector<index>& ngrams,
        const int length) const;
    bool readSequence(
        std::istream& in, std::vector<index>& ngrams,
        const int length,
        bool add_noise,
        std::mt19937_64&) const;
    bool readSequence(std::string& word,
                      std::vector<index>& ngrams) const;
};
}
