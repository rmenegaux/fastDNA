/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <iostream>
#include <queue>
#include <iomanip>
// needed for kallisto
#include <getopt.h>
#include <sstream>
#include <sys/stat.h>

#include "fasttext.h"
#include "args.h"
#include "common.h"

using namespace fasttext;

void printUsage() {
  std::cerr
    << "usage: fastdna <command> <args>\n\n"
    << "The commands supported by fastdna are:\n\n"
    << "  supervised              train a supervised classifier\n"
    << "  quantize                quantize a model to reduce the memory usage\n"
    << "  test                    evaluate a supervised classifier\n"
    << "  predict                 predict most likely labels\n"
    << "  predict-prob            predict most likely labels with probabilities\n"
    // << "  skipgram                train a skipgram model\n"
    // << "  cbow                    train a cbow model\n"
    << "  print-word-vectors      print word vectors given a trained model\n"
    << "  print-sentence-vectors  print sentence vectors given a trained model\n"
    // << "  print-ngrams            print ngrams given a trained model and word\n"
    // << "  nn                      query for nearest neighbors\n"
    // << "  analogies               query for analogies\n"
    // << "  dump                    dump arguments,dictionary,input/output vectors\n"
    << std::endl;
}

void printQuantizeUsage() {
  std::cerr
    << "usage: fastdna quantize <args>"
    << std::endl;
}

void printTestUsage() {
  std::cerr
    << "usage: fastdna test <model> <test-data> [<k>] [<th>]\n\n"
    << "  <model>      model filename\n"
    << "  <test-data>  test data filename (if -, read from stdin)\n"
    << "  <model>      test labels filename\n"
    << "  <index>      kallisto index\n"
    << "  <k>          (optional; 1 by default) predict top k labels\n"
    << "  <th>         (optional; 0.0 by default) probability threshold\n"
    << std::endl;
}

void printPredictUsage() {
  std::cerr
    << "usage: fastdna predict[-prob] <model> <test-data> [<k>] [<th>]\n\n"
    << "  <model>      model filename\n"
    << "  <test-data>  test data filename (if -, read from stdin)\n"
    << "  <index>      kallisto index\n"
    << "  <k>          (optional; 1 by default) predict top k labels\n"
    << "  <th>         (optional; 0.0 by default) probability threshold\n"
    << std::endl;
}

void printPrintWordVectorsUsage() {
  std::cerr
    << "usage: fastdna print-word-vectors <model>\n\n"
    << "  <model>      model filename\n"
    << std::endl;
}

void printPrintSentenceVectorsUsage() {
  std::cerr
    << "usage: fastdna print-sentence-vectors <model>\n\n"
    << "  <model>      model filename\n"
    << std::endl;
}

void printPrintNgramsUsage() {
  std::cerr
    << "usage: fastdna print-ngrams <model> <word>\n\n"
    << "  <model>      model filename\n"
    << "  <word>       word to print\n"
    << std::endl;
}

void quantize(const std::vector<std::string>& args) {
  Args a = Args();
  if (args.size() < 3) {
    printQuantizeUsage();
    a.printHelp();
    exit(EXIT_FAILURE);
  }
  a.parseArgs(args);
  FastText fasttext;
  // parseArgs checks if a->output is given.
  fasttext.loadModel(a.output + ".bin");
  fasttext.quantize(a);
  fasttext.saveModel();
  exit(0);
}

void printNNUsage() {
  std::cout
    << "usage: fastdna nn <model> <k>\n\n"
    << "  <model>      model filename\n"
    << "  <k>          (optional; 10 by default) predict top k labels\n"
    << std::endl;
}

void printAnalogiesUsage() {
  std::cout
    << "usage: fastdna analogies <model> <k>\n\n"
    << "  <model>      model filename\n"
    << "  <k>          (optional; 10 by default) predict top k labels\n"
    << std::endl;
}

void printDumpUsage() {
  std::cout
    << "usage: fastdna dump <model> <option>\n\n"
    << "  <model>      model filename\n"
    << "  <option>     option from args,dict,input,output"
    << std::endl;
}

void printIndexUsage() {
  std::cout
    << "Builds a kallisto index\n\n"
    << "Usage: fastdna index [arguments] FASTA-files\n"
    << "Required argument:\n"
    << "-i, --index=STRING          Filename for the kallisto index to be constructed\n\n"
    << "Optional argument:\n"
    << "-k, --kmer-size=INT         k-mer (odd) length (default: 31, max value: " << (Kmer::MAX_K-1) << ")\n"
    << "    --make-unique           Replace repeated target names with unique names\n"
    << std::endl;
}

void test(const std::vector<std::string>& args) {
  if (args.size() < 6 || args.size() > 8) {
    printTestUsage();
    exit(EXIT_FAILURE);
  }
  int32_t k = 1;
  real threshold = 0.0;
  if (args.size() > 6) {
    k = std::stoi(args[6]);
    if (args.size() == 8) {
      threshold = std::stof(args[7]);
    }
  }

  FastText fasttext;
  // std::cerr << "Loading Model" << std::endl;
  fasttext.loadModel(args[2]);
  fasttext.loadIndex(args[5]);
  // std::cerr << "Model Loaded" << std::endl;

  std::tuple<int64_t, double, double> result;
  std::string infile = args[3];
  std::string labelfile = args[4];
  if (infile == "-") {
    // result = fasttext.test(std::cin, k, threshold);
  } else {
    std::ifstream ifs(infile);
    if (!ifs.is_open()) {
      std::cerr << "Test file cannot be opened!" << std::endl;
      exit(EXIT_FAILURE);
    }
    std::ifstream labels(labelfile);
    if (!ifs.is_open()) {
      std::cerr << "Label file cannot be opened!" << std::endl;
      exit(EXIT_FAILURE);
    }
    result = fasttext.test(ifs, labels, k, threshold);
    ifs.close();
  }
  std::cout << "N" << "\t" << std::get<0>(result) << std::endl;
  std::cout << std::setprecision(3);
  std::cout << "P@" << k << "\t" << std::get<1>(result) << std::endl;
  std::cout << "R@" << k << "\t" << std::get<2>(result) << std::endl;
  std::cerr << "Number of examples: " << std::get<0>(result) << std::endl;
}

void predict(const std::vector<std::string>& args) {
  if (args.size() < 5 || args.size() > 7) {
    printPredictUsage();
    exit(EXIT_FAILURE);
  }
  int32_t k = 1;
  real threshold = 0.0;
  if (args.size() > 5) {
    k = std::stoi(args[5]);
    if (args.size() == 7) {
      threshold = std::stof(args[6]);
    }
  }

  bool print_prob = args[1] == "predict-prob";
  FastText fasttext;
  fasttext.loadModel(std::string(args[2]));
  fasttext.loadIndex(args[4]);

  std::string infile(args[3]);
  if (infile == "-") {
    fasttext.predict(std::cin, k, print_prob, threshold);
  } else {
    std::ifstream ifs(infile);
    if (!ifs.is_open()) {
      std::cerr << "Input file cannot be opened!" << std::endl;
      exit(EXIT_FAILURE);
    }
    fasttext.predict(ifs, k, print_prob, threshold);
    ifs.close();
  }

  exit(0);
}

void printWordVectors(const std::vector<std::string> args) {
  if (args.size() != 3) {
    printPrintWordVectorsUsage();
    exit(EXIT_FAILURE);
  }
  FastText fasttext;
  fasttext.loadModel(std::string(args[2]));
  std::string word;
  Vector vec(fasttext.getDimension());
  int i = 0;
  while (!std::cin.eof()) {
    std::cerr << "\rRead sequence n" << i << std::flush;
    fasttext.getWordVector(vec, std::cin);
    std::cout << vec << std::endl;
    i++;
    // std::cout << word << " " << vec << std::endl;
  }
  exit(0);
}

void printSentenceVectors(const std::vector<std::string> args) {
  // if (args.size() != 3) {
  //   printPrintSentenceVectorsUsage();
  //   exit(EXIT_FAILURE);
  // }
  // FastText fasttext;
  // fasttext.loadModel(std::string(args[2]));
  // Vector svec(fasttext.getDimension());
  // while (std::cin.peek() != EOF) {
  //   fasttext.getSentenceVector(std::cin, svec);
  //   // Don't print sentence
  //   std::cout << svec << std::endl;
  // }
  // exit(0);
}

void printNgrams(const std::vector<std::string> args) {
  if (args.size() != 4) {
    printPrintNgramsUsage();
    exit(EXIT_FAILURE);
  }
  FastText fasttext;
  fasttext.loadModel(std::string(args[2]));
  fasttext.ngramVectors(std::string(args[3]));
  exit(0);
}

void nn(const std::vector<std::string> args) {
  int32_t k;
  if (args.size() == 3) {
    k = 10;
  } else if (args.size() == 4) {
    k = std::stoi(args[3]);
  } else {
    printNNUsage();
    exit(EXIT_FAILURE);
  }
  FastText fasttext;
  fasttext.loadModel(std::string(args[2]));
  std::string queryWord;
  std::shared_ptr<const Dictionary> dict = fasttext.getDictionary();
  Vector queryVec(fasttext.getDimension());
  Matrix wordVectors(dict->nwords(), fasttext.getDimension());
  std::cerr << "Pre-computing word vectors...";
  fasttext.precomputeWordVectors(wordVectors);
  std::cerr << " done." << std::endl;
  std::set<std::string> banSet;
  std::cout << "Query word? ";
  std::vector<std::pair<real, std::string>> results;
  while (std::cin >> queryWord) {
    banSet.clear();
    banSet.insert(queryWord);
    fasttext.getWordVector(queryVec, queryWord);
    fasttext.findNN(wordVectors, queryVec, k, banSet, results);
    for (auto& pair : results) {
      std::cout << pair.second << " " << pair.first << std::endl;
    }
    std::cout << "Query word? ";
  }
  exit(0);
}

void analogies(const std::vector<std::string> args) {
  int32_t k;
  if (args.size() == 3) {
    k = 10;
  } else if (args.size() == 4) {
    k = std::stoi(args[3]);
  } else {
    printAnalogiesUsage();
    exit(EXIT_FAILURE);
  }
  FastText fasttext;
  fasttext.loadModel(std::string(args[2]));
  fasttext.analogies(k);
  exit(0);
}

void train(const std::vector<std::string> args) {
  Args a = Args();
  a.parseArgs(args);
  FastText fasttext;
  std::ofstream ofs(a.output+".bin");
  if (!ofs.is_open()) {
    throw std::invalid_argument(a.output + ".bin cannot be opened for saving.");
  }
  ofs.close();
  fasttext.train(a);
  fasttext.saveModel();
  fasttext.saveVectors();
  if (a.saveOutput) {
    fasttext.saveOutput();
  }
}

void dump(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    printDumpUsage();
    exit(EXIT_FAILURE);
  }

  std::string modelPath = args[2];
  std::string option = args[3];

  FastText fasttext;
  fasttext.loadModel(modelPath);
  if (option == "args") {
    fasttext.getArgs().dump(std::cout);
  } else if (option == "dict") {
    fasttext.getDictionary()->dump(std::cout);
  } else if (option == "input") {
    if (fasttext.isQuant()) {
      std::cerr << "Not supported for quantized models." << std::endl;
    } else {
      fasttext.getInputMatrix()->dump(std::cout);
    }
  } else if (option == "output") {
    if (fasttext.isQuant()) {
      std::cerr << "Not supported for quantized models." << std::endl;
    } else {
      fasttext.getOutputMatrix()->dump(std::cout);
    }
  } else {
    printDumpUsage();
    exit(EXIT_FAILURE);
  }
}

bool CheckOptionsIndex(ProgramOptions& opt) {

  bool ret = true;

  if (opt.k <= 1 || opt.k >= Kmer::MAX_K) {
    std::cerr << "Error: invalid k-mer length " << opt.k << ", minimum is 3 and  maximum is " << (Kmer::MAX_K -1) << std::endl;
    ret = false;
  }

  if (opt.k % 2 == 0) {
    std::cerr << "Error: k needs to be an odd number" << std::endl;
    ret = false;
  }

  if (opt.transfasta.empty()) {
    std::cerr << "Error: no FASTA files specified" << std::endl;
    ret = false;
  } else {

    for (auto& fasta : opt.transfasta) {
      // we want to generate the index, check k, index and transfasta
      struct stat stFileInfo;
      auto intStat = stat(fasta.c_str(), &stFileInfo);
      if (intStat != 0) {
        std::cerr << "Error: FASTA file not found " << fasta << std::endl;
        ret = false;
      }
    }
  }

  if (opt.index.empty()) {
    std::cerr << "Error: need to specify kallisto index name" << std::endl;
    ret = false;
  }

  return ret;
}

void ParseOptionsIndex(int argc, char **argv, ProgramOptions& opt) {
  int verbose_flag = 0;
  int make_unique_flag = 0;
  const char *opt_string = "i:k:";
  static struct option long_options[] = {
    // long args
    {"verbose", no_argument, &verbose_flag, 1},
    {"make-unique", no_argument, &make_unique_flag, 1},
    // short args
    {"index", required_argument, 0, 'i'},
    {"kmer-size", required_argument, 0, 'k'},
    {0,0,0,0}
  };
  int c;
  int option_index = 0;
  while (true) {
    c = getopt_long(argc,argv,opt_string, long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
    case 0:
      break;
    case 'i': {
      opt.index = optarg;
      break;
    }
    case 'k': {
      std::stringstream(optarg) >> opt.k;
      break;
    }
    default: break;
    }
  }

  if (verbose_flag) {
    opt.verbose = true;
  }
  if (make_unique_flag) {
    opt.make_unique = true;
  }

  for (int i = optind; i < argc; i++) {
    opt.transfasta.push_back(argv[i]);
  }
}

void makeIndex(int argc, char **argv) {
  ProgramOptions opt;
  if (argc==2) {
    printIndexUsage();
  }
  ParseOptionsIndex(argc-1,argv+1,opt);
  if (!CheckOptionsIndex(opt)) {
    printIndexUsage();
    exit(EXIT_FAILURE);
  } else {
    // create an index
    Kmer::set_k(opt.k);
    KmerIndex index(opt);
    index.BuildTranscripts(opt);
    index.write(opt.index);
  }
}

int main(int argc, char** argv) {
  std::vector<std::string> args(argv, argv + argc);
  if (args.size() < 2) {
    printUsage();
    exit(EXIT_FAILURE);
  }
  std::string command(args[1]);
  if (command == "skipgram" || command == "cbow" || command == "supervised") {
    train(args);
  } else if (command == "test") {
    test(args);
  } else if (command == "quantize") {
    quantize(args);
  } else if (command == "print-word-vectors") {
    printWordVectors(args);
  } else if (command == "print-sentence-vectors") {
    printSentenceVectors(args);
  } else if (command == "print-ngrams") {
    printNgrams(args);
  } else if (command == "nn") {
    nn(args);
  } else if (command == "analogies") {
    analogies(args);
  } else if (command == "predict" || command == "predict-prob") {
    predict(args);
  } else if (command == "dump") {
    dump(args);
  } else if (command == "index") {
    makeIndex(argc, argv);
  } else {
    printUsage();
    exit(EXIT_FAILURE);
  }
  return 0;
}
