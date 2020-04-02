from sklearn.metrics import accuracy_score
import numpy as np
import argparse


parser = argparse.ArgumentParser(description=
    '''
    Compute average species-level recall and precision.
    ''')

parser.add_argument("predictions", nargs='+',
                    help="predicted labels, text file with one label per line",
                    type=str)

parser.add_argument("truth", nargs='+',
                    help="ground truth labels, text file with one label per line",
                    type=str)


def evaluate_predictions(pred_labels, true_labels, verbose=0):
    ''' 
    Computes average species-level precision and recall
    '''

    y_true = np.genfromtxt(true_labels, dtype=str)

    with open(pred_labels) as f:
        y_pred = f.readlines()
    y_pred = np.array([x.strip() for x in y_pred])

    unique_true = np.unique(y_true)

    rec_per_species = np.array(
        [accuracy_score(y_pred[y_true==k], y_true[y_true==k])
            for k in unique_true]
        )
    recall = np.mean(rec_per_species)

    acc_per_species = np.array(
        [accuracy_score(y_pred[y_pred==k], y_true[y_pred==k])
            for k in unique_true if (y_pred==k).any()]
        )
    precision = np.mean(acc_per_species)

    if verbose > 0:
        print('''Recall: {:.2%}  Precision: {:.2%}'''.format(recall, precision))
        print('''Number of predicted species {}'''.format(len(np.unique(y_pred))))

    return {'Recall': recall, 'Precision': precision}



if __name__ == "__main__":
    args = parser.parse_args()
    evaluate_predictions(args.predictions[0], args.truth[0], verbose=1)