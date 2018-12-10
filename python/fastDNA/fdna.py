import argparse
import os

from fastDNA import FastDNA
from utils import make_model_name


parser = argparse.ArgumentParser(description="train/predict/quantize fdna model")

parser.add_argument("-output_dir", help="output directory",
                    type=str, default="output/")
# Type of action
parser.add_argument("-train", help="train model",
                    action="store_true")
parser.add_argument("-quantize", help="quantize model",
                    action="store_true")
parser.add_argument("-predict", help="make and evaluate predictions",
                    action="store_true")
parser.add_argument("-eval", help="make and evaluate predictions",
                    action="store_true")
parser.add_argument("-predict_quant",
                    help="make and evaluate predictions with quantized model",
                    action="store_true")
parser.add_argument("-verbose",
                    help="output verbosity, 0 1 or 2",
                    type=int, default=1)
# Dataset
parser.add_argument("-train_dataset", help="training dataset, fasta file containing full genomes",
                    type=str, default="example_data/train.fasta")
parser.add_argument("-train_labels", help="training labels, text file containing as many labels as there are training genomes",
                    type=str, default="example_data/train.labels")
parser.add_argument("-test_dataset", help="testing dataset, fasta file containing fragments",
                    type=str, default="example_data/test.fasta")
parser.add_argument("-test_labels", help="testing dataset, fasta file containing fragments",
                    type=str, default="example_data/test.labels")
# Model parameters
parser.add_argument("-model_name", help="imposed model name",
                    type=str)
parser.add_argument("-threads", help="number of threads",
                    type=int, default=4)
parser.add_argument("-d", help="embedding dimension",
                    type=int, default=10)
parser.add_argument("-k", help="k-mer length",
                    type=int, default=12)
parser.add_argument("-e", help="number of training epochs",
                    type=int, default=1)
parser.add_argument("-lr", help="learning rate",
                    type=float, default=0.1)
parser.add_argument("-noise", help="level of training noise",
                    type=float, default=0)
parser.add_argument("-skip", help="number of character steps",
                    type=int, default=0)
parser.add_argument("-L", help="training read length",
                    type=int, default=200)
parser.add_argument("-freeze", help="freeze the embeddings",
                    action="store_true")
# Additional files
parser.add_argument("-pretrained_vectors", help="pretrained vectors .vec files",
                    type=str)

args = parser.parse_args()

# -----------------
# General file Paths
# -----------------
fastdna_path = '../fastdna'

# -----------------
# FastDNA object
# -----------------
ft = FastDNA(fastdna_path, args.verbose)

# -----------
# Model name
# -----------
if args.model_name:
    model_name = args.model_name
else:
    model_name = make_model_name(args)

# -------------------------
# Model-specific file paths
# -------------------------
def mkdir(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)
    return directory

model_dir = mkdir(os.path.join(args.output_dir, 'models'))
pred_dir = mkdir(os.path.join(args.output_dir, 'predictions'))

model_path = os.path.join(model_dir, model_name)
pred_path = os.path.join(pred_dir, '{}.pred'.format(model_name))
pred_path_quant = os.path.join(pred_dir, '{}.quant.pred'.format(model_name))

pretrained_vectors = args.pretrained_vectors if args.pretrained_vectors else ""


if args.train:
    ft.train(
        model_path, args.train_dataset, args.train_labels,
        dim=args.d,
        k=args.k,
        length=args.L,
        epoch=args.e,
        lr=args.lr,
        noise=args.noise,
        skip=args.skip,
        freeze=args.freeze,
        pretrained_vectors=pretrained_vectors,
        threads=args.threads,
        )

if args.eval:
    results = ft.predict_eval(
        '{}.bin'.format(model_path),
        args.test_dataset,
        args.test_labels,
        pred_path,
    )

elif args.predict:
    ft.make_predictions(
        '{}.bin'.format(model_path),
        args.test_dataset,
        pred_path,
    )

if args.quantize:
    ft.quantize(model_path, args.train_dataset)

if args.predict_quant:
    pass

