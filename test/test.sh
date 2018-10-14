# -----------------
# File Paths
# -----------------
data_path="data/"
output_path="output/models"

mkdir -p $output_path

train_dataset="$data_path/train/A1.train.fasta"
train_labels="$data_path/train/A1.train.taxid"
test_dataset="$data_path/test/A1.test.fasta"
test_labels="$data_path/test/A1.test.taxid"

fastdna=../fastdna

threads=4 # number of threads for training
d=10 # embedding dimension
k=10 # k-mer length
e=1 # number of epochs
L=100 # training read length,

model_name="fdna_k${k}_d${d}_e${e}"
model_path="$output_path/$model_name"

# Train a supervised model
echo "Training model $model_name"
$fastdna supervised -input $train_dataset -labels $train_labels -output $model_path -minn $k -dim $d -epoch $e -thread $threads

# Test the model
echo "Testing model $model_name"
$fastdna test $model_path.bin $test_dataset $test_labels