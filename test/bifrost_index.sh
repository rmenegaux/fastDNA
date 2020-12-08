# Build a bifrost index

root_folder=/Users/romainmenegaux/These
bifrost=$root_folder/bifrost/build/src/Bifrost

# FASTA file to index
db="small"
data=$root_folder/these_romain/data/$db-DB/reference-dataset/train_$db-db.fasta
data=data/train/A1.train.fasta
db=A1

# k-mer size (must be odd)
k=13
# Number of threads
threads=4

# Name of the output file
index_file=output/bifrost_index_$db_k$k

# Build kallisto index
$bifrost build -t $threads -k $k -o $index_file -s $data -v
