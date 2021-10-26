# Split fasta into equal overlapping chunks

input_fasta=/Users/romainmenegaux/These/fastDNA/test/data/train/A1.train.fasta
output=/Users/romainmenegaux/These/these_romain/DNAPiece/output.fa
fragment_length=2048
overlap=100

shred.sh in=$input_fasta out=$output length=$fragment_length minlength=1 overlap=$overlap

