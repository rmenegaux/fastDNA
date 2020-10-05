import argparse
import os, sys
sys.path.append(os.path.join(os.path.dirname(__file__), "python", "fastDNA"))

from fastDNA import FastDNA
from utils import make_model_name


parser = argparse.ArgumentParser(description="train, predict and/or quantize fdna model")

# Type of action
parser.add_argument("-train", help="train model",
                    action="store_true")
parser.add_argument("-quantize", help="quantize model",
                    action="store_true")
parser.add_argument("-predict", help="make predictions",
                    action="store_true")
parser.add_argument("-eval", help="make and evaluate predictions",
                    action="store_true")
parser.add_argument("-predict_quant",
                    help="make and evaluate predictions with quantized model",
                    action="store_true")
# Datasets
parser.add_argument("-train_fasta", help="training dataset, fasta file containing full genomes",
                    type=str, default="example_data/train.fasta")
parser.add_argument("-train_labels", help="training labels, text file containing as many labels as there are training genomes",
                    type=str)
parser.add_argument("-test_fasta", help="testing dataset, fasta file containing reads",
                    type=str)
parser.add_argument("-test_labels", help="testing labels, text file containing as many labels as there are reads",
                    type=str)
# Output directory
parser.add_argument("-output_dir", help="output directory",
                    type=str, default="output/")
# Model parameters
parser.add_argument("-model_name", help="optional user-defined model name",
                    type=str)
# Training parameters
parser.add_argument("-loss", help="loss function (softmax (default) or hs)",
                    type=str, default='softmax')
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
parser.add_argument("-noise", help="level of training noise, percent of random mutations",
                    type=float, default=0)
parser.add_argument("-L", help="training read length",
                    type=int, default=200)
parser.add_argument("-freeze", help="freeze the embeddings",
                    action="store_true")
# Prediction parameters
parser.add_argument("-threshold", help="minimum probability to make a prediction",
                    type=float, default=0.)
parser.add_argument("-paired", help="Add option if preicting on paired-end input",
                    action="store_true")
# Additional files
parser.add_argument("-taxonomy", help="precomputed tree for hierarchical clustering",
                    type=str, default="")
parser.add_argument("-pretrained_vectors", help="pretrained vectors .vec files",
                    type=str)
parser.add_argument("-verbose",
                    help="output verbosity, 0 1 or 2",
                    type=int, default=1)

args = parser.parse_args()

# ----------------
# Check arguments
# ----------------
actions = ['train', 'eval', 'predict', 'predict_quant', 'quantize']

if not any([getattr(args, a) for a in actions]):
    parser.error('At least one of these arguments is required: {}'.format(actions))

def check_required_args(arg_name, required_args):
    if arg_name in vars(args):
        for a in required_args:
            if a not in vars(args):
                parser.error('Argument `-{}` requires argument `-{}`, none was provided'.format(arg_name, a))

check_required_args('train', ['train_fasta', 'train_labels'])
check_required_args('eval', ['test_labels'])
for a in ['eval', 'predict', 'predict_quant']:
    check_required_args(a, ['test_fasta'])

# -----------------
# General file Paths
# -----------------
fastdna_path = os.path.join(os.path.dirname(__file__), 'fastdna')

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

mkdir(args.output_dir)
model_dir = mkdir(os.path.join(args.output_dir, 'models'))
pred_dir = mkdir(os.path.join(args.output_dir, 'predictions'))

model_path = os.path.join(model_dir, model_name)
pred_path = os.path.join(pred_dir, '{}.pred'.format(model_name))
pred_path_quant = os.path.join(pred_dir, '{}.quant.pred'.format(model_name))

pretrained_vectors = args.pretrained_vectors if args.pretrained_vectors else ""


if args.train:
    ft.train(
        model_path, args.train_fasta, args.train_labels,
        dim=args.d,
        k=args.k,
        length=args.L,
        epoch=args.e,
        lr=args.lr,
        noise=args.noise,
        freeze=args.freeze,
        pretrained_vectors=pretrained_vectors,
        threads=args.threads,
        loss=args.loss,
        taxonomy=args.taxonomy,
        )

if args.eval:
    results = ft.predict_eval(
        '{}.bin'.format(model_path),
        args.test_fasta,
        args.test_labels,
        pred_path,
        paired=args.paired,
    )

elif args.predict:
    ft.make_predictions(
        '{}.bin'.format(model_path),
        args.test_fasta,
        pred_path,
        paired=args.paired,
    )

if args.quantize:
    ft.quantize(model_path, args.train_fasta)

if args.predict_quant:
    ft.make_predictions(
        '{}.ftz'.format(model_path),
        args.test_fasta,
        pred_path,
        paired=args.paired,
    )

