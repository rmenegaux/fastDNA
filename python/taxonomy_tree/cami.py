from utils import test_datasets
import subprocess
import os

predictions=[
"/mnt/data40T/rmenegaux/these_romain/fasttext/output/predictions/fdna_cami_k14_d50_e100_lr0.1_n4.0_fragments.pred",
"output/predictions/fdna_k14_d50_e100_lr0.1_hsloss_tax.pred",
]
sample_name="large"

convert_to_cami="/mnt/data40T/rmenegaux/these_romain/cami_evaluation/to_biobox.sh"

test_dataset = test_datasets[sample_name]
seqids = test_dataset['seqids']
ground_truth = test_dataset['gold_standard']

for prediction in predictions:
    prediction_cami_format=os.path.join(output_dir, os.path.basename(prediction)+'.cami')
    subprocess.check_call(
        '{} -s {} -o {} -t {} -n {}'.format(
            convert_to_cami,
            seqids,
            prediction_cami_format,
            ground_truth,
            sample_name), shell=True)
    command += " {}".format(prediction)
command += " -ids {} -truth {} -name {}".format(test_dataset['seqids'], test_dataset['gold_standard'], sample_name)
print(command)
subprocess.check_call(command, shell=True)
