# Build a kallisto index

root_folder=/Users/romainmenegaux/These
root_folder=/mnt/data40T/rmenegaux/
kallisto=$root_folder/kallisto/build/src/kallisto
kallisto=$root_folder/fastkallisto/fastdna

# FASTA file to index
db="cami"
data=$root_folder/these_romain/data/$db-DB/reference-dataset/train_$db-db.fasta
data=/mnt/data40T/rmenegaux/these_romain/data/full_fasta.fna

# k-mer size (must be odd)
k=15

# Name of the output file
index_file=$root_folder/these_romain/data/kallisto_index_$db_k$k

# Build kallisto index
$kallisto index $data --make-unique -i $index_file -k $k
