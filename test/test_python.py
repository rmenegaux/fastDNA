import subprocess
import os

# -----------------
# File Paths
# -----------------
data_path = 'data/'
output_path = 'output/'
fdna_py = '../fdna.py'

train_dataset = os.path.join(data_path, 'train', 'A1.train.fasta')
train_labels = os.path.join(data_path, 'train', 'A1.train.taxid')
test_dataset = os.path.join(data_path, 'test', 'A1.test.fasta')
test_labels = os.path.join(data_path, 'test', 'A1.test.taxid')

args = {
"output_dir": output_path, # Directory for saving model and predictions
"train" : True, # train the model
"train_fasta": train_dataset,
"train_labels": train_labels,
"test_fasta": test_dataset,
"test_labels": test_labels,
"quantize": False, # quantize the mode
"predict": False, # make predictions
"eval": True, # make and evaluate predictions
"predict_quant": False, # evaluate the quantized model
"verbose": 1,
"model_name": False, # impose a model name
"threads": 4, # number of threads for training
"d": 10, # embedding dimension
"k": 10, # k-mer length
"e": 1, # number of epochs
"lr": 0.1, # learning rate
"noise": 0, # level of training noise
"L": 100, # training read length,
"freeze": False, # freeze the embeddings,
"pretrained_vectors": False, # pretrained vectors .vec files,
"paired": False, # paired end reads,
"threshold": 0, # minimum probability to make a prediction,
}

# To reproduce results from the paper:
# args.update({
# "d": 100,
# "k": 13,
# "e": 200,
# "lr": 0.1,
# "noise": 4,
# "L": 200,
# })

python_command = 'python {}'.format(fdna_py)
for k, v in args.items():
    if v is True:
        python_command += ' -{}'.format(k)
    elif v is not False:
        python_command += ' -{} {}'.format(k, v)

# Launch command
subprocess.check_call(
    python_command,
    shell=True
    )
