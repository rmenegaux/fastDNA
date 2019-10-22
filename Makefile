#
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
#

CXX = c++
CXXFLAGS = -pthread -std=c++0x -march=native
# bifrost binaries
OBJS = Kmer.cpp.o GFA_Parser.cpp.o FASTX_Parser.cpp.o roaring.c.o KmerIterator.cpp.o UnitigMap.cpp.o TinyBitMap.cpp.o BlockedBloomFilter.cpp.o ColorSet.cpp.o CompressedSequence.cpp.o CompressedCoverage.cpp.o
OBJS = $(OBJS) args.o dictionary.o productquantizer.o matrix.o qmatrix.o vector.o model.o utils.o fasttext.o 
INCLUDES = -I.

opt: CXXFLAGS += -O3 -funroll-loops -DNDEBUG -lz
opt: fastdna

debug: CXXFLAGS += -g -O0 -fno-inline
debug: fastdna

# Common.o: src/Common.cpp src/Common.hpp
# 	$(CXX) $(CXXFLAGS) -c src/Common.cpp
# 
# BlockedBloomFilter.o: src/BlockedBloomFilter.cpp src/libdivide.h src/libpopcnt.h src/BlockedBloomFilter.hpp
# 	$(CXX) $(CXXFLAGS) -c src/BlockedBloomFilter.cpp
# 
# FASTX_Parser.o: src/FASTX_Parser.cpp src/FASTX_Parser.hpp
# 	$(CXX) $(CXXFLAGS) -c src/FASTX_Parser.cpp
# 
# File_Parser.o: src/File_Parser.cpp src/FASTX_Parser.hpp src/GFA_Parser.hpp src/File_Parser.hpp
# 	$(CXX) $(CXXFLAGS) -c src/File_Parser.cpp
# 
# GFA_Parser.o: src/File_Parser.cpp src/FASTX_Parser.hpp src/GFA_Parser.hpp src/File_Parser.hpp
# 	$(CXX) $(CXXFLAGS) -c src/GFA_Parser.cpp
# 
# hash.o: src/hash.cpp src/hash.hpp
# 	$(CXX) $(CXXFLAGS) -c src/hash.cpp
# 
# Kmer.o: src/Kmer.cpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/Kmer.cpp
# 
# KmerHashTable.o: src/Kmer.cpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/KmerHashTable.cpp
# 
# KmerStream.o: src/Kmer.cpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/KmerStream.cpp
# 
# Lock.o: src/Kmer.cpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/Lock.cpp
# 
# minHashIterator.o: src/Kmer.cpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/minHashIterator.cpp
# 
# RepHash.o: src/Kmer.cpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/RepHash.cpp
# 
# TinyVector.o: src/Kmer.cpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/TinyVector.cpp
# 
# Unitig.o: src/UnitigMap.cpp src/UnitigMap.hpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/Unitig.cpp
# 
# UnitigMap.o: src/UnitigMap.cpp src/UnitigMap.hpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/UnitigMap.cpp
# 
# UnitigIterator.o: src/UnitigMap.cpp src/UnitigMap.hpp src/Common.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/UnitigIterator.cpp
# 
# KmerIterator.o: src/KmerIterator.cpp src/KmerIterator.hpp src/Kmer.hpp
# 	$(CXX) $(CXXFLAGS) -c src/KmerIterator.cpp
# 
# CompactedDBG.o: src/CompactedDBG.tcc src/CompactedDBG.hpp src/BlockedBloomFilter.hpp src/Common.hpp src/File_Parser.hpp src/FASTX_Parser.hpp src/GFA_Parser.hpp src/Kmer.hpp src/KmerHashTable.hpp src/KmerIterator.hpp src/KmerStream.hpp src/Lock.hpp src/minHashIterator.hpp src/RepHash.hpp src/TinyVector.hpp src/Unitig.hpp src/UnitigIterator.hpp src/UnitigMap.hpp
# 	$(CXX) $(CXXFLAGS) -c src/CompactedDBG.tcc
# 
args.o: src/args.cc src/args.h
	$(CXX) $(CXXFLAGS) -c src/args.cc

dictionary.o: src/dictionary.cc src/dictionary.h src/args.h src/*.hpp
	$(CXX) $(CXXFLAGS) -c src/dictionary.cc

productquantizer.o: src/productquantizer.cc src/productquantizer.h src/utils.h
	$(CXX) $(CXXFLAGS) -c src/productquantizer.cc

matrix.o: src/matrix.cc src/matrix.h src/utils.h
	$(CXX) $(CXXFLAGS) -c src/matrix.cc

qmatrix.o: src/qmatrix.cc src/qmatrix.h src/utils.h
	$(CXX) $(CXXFLAGS) -c src/qmatrix.cc

vector.o: src/vector.cc src/vector.h src/utils.h
	$(CXX) $(CXXFLAGS) -c src/vector.cc

model.o: src/model.cc src/model.h src/args.h
	$(CXX) $(CXXFLAGS) -c src/model.cc

utils.o: src/utils.cc src/utils.h
	$(CXX) $(CXXFLAGS) -c src/utils.cc

fasttext.o: src/fasttext.cc src/*.h src/*.hpp
	$(CXX) $(CXXFLAGS) -c src/fasttext.cc

fastdna: $(OBJS) src/fasttext.cc
	$(CXX) $(CXXFLAGS) $(OBJS) src/main.cc -o fastdna

clean:
	rm -rf *.o fasttext
