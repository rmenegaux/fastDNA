# Build a kallisto index

root_folder=/Users/romainmenegaux/These
kallisto=$root_folder/kallisto/build/src/kallisto

# FASTA file to index
db="small"
data=$root_folder/these_romain/data/$db-DB/reference-dataset/train_$db-db.fasta

# k-mer size (must be odd)
k=13

# Name of the output file
index_file=$root_folder/these_romain/data/kallisto_index_$db_k$k

# Build kallisto index
$kallisto index $data --make-unique -i $index_file -k $k