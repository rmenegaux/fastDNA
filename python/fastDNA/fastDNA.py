import time
import subprocess


def timed_check_call(command, name='Checked call', verbose=1):
    ''''''
    if verbose > 0:
        print('{} started'.format(name))
    if (verbose > 1):
        print(command)

    start = time.time()
    subprocess.check_call(command, shell=True)
    end = time.time()
    
    if verbose > 0:
        print('{} finished, elapsed time {:d}s'.format(name, int(end-start)))

    return end-start


class FastDNA():

    def __init__(self, fastdna_path, verbose=1):
        self.fastdna = fastdna_path
        self.verbose = verbose

    def train(self, model_path, train_data, train_labels,
              dim=10, k=10,
              length=200, epoch=1, lr=0.1, noise=0, skip=1,
              freeze=False, pretrained_vectors="", previous_model="",
              threads=4):
        '''
        Trains a fastdna model
        '''
        # Basic training command
        command = '{} supervised -input {} -output {} -labels {}'.format(
            self.fastdna, train_data, model_path, train_labels)

        # Parameters for training
        parameters = ' -epoch {} -dim {} -lr {} -minn {} -maxn {} -thread {} -length {}'.format(
            epoch, dim, lr, k, k, threads, length)
        if noise > 0:
            parameters += ' -noise {}'.format(int(noise * 1000))
        if skip != 1:
            parameters += ' -skip {}'.format(skip)
        if freeze:
            parameters += ' -freezeEmbeddings'
        # Loading previous .vec or .bin
        if len(pretrained_vectors) > 0:
            parameters += ' -pretrainedVectors {}'.format(pretrained_vectors)
        if len(previous_model) > 0:
            parameters += ' -loadModel {}'.format(previous_model)

        command += parameters

        readout = 'Training model {}'.format(model_path.split('/')[-1])
        return timed_check_call(command, name=readout, verbose=self.verbose)


    def quantize(self, model_path, train_data):
        '''
        Quantize the model given by model_path
        '''
        command = '{} quantize -input {} -output {} -qnorm'.format(self.fastdna, train_data, model_path)

        readout = 'Quantizing model {}'.format(model_path.split('/')[-1])
        return timed_check_call(command, name=readout, verbose=self.verbose)


    def make_predictions(self, model_path, input_data, output_path):
        ''' 
        Predicts the labels for `input_data`
        Stores the results in `output_path`
        '''
        command = '{} predict {} {} > {}'.format(
            self.fastdna, model_path, input_data, output_path)

        readout = 'Making predictions {}'.format(model_path.split('/')[-1])
        return timed_check_call(command, name=readout, verbose=self.verbose)


    def evaluate_predictions(self, pred_labels, true_labels):
        ''' 
        Computes average species-level precision and recall
        '''
        from sklearn.metrics import accuracy_score
        import numpy as np

        y_true = np.genfromtxt(true_labels, dtype=str)

        with open(pred_labels) as f:
            y_pred = f.readlines()
        y_pred = np.array([x.strip() for x in y_pred])

        rec_per_species = {k: accuracy_score(y_pred[y_true==k],
                                             y_true[y_true==k])
                           for k in np.unique(y_true)}
        rec = np.mean(rec_per_species.values())
        acc_per_species = {k: accuracy_score(y_pred[y_pred==k],
                                             y_true[y_pred==k])
                           for k in np.unique(y_true) if (y_pred==k).any()}
        prec = np.mean(acc_per_species.values())

        if self.verbose > 0:
            print('''Recall: {:.2%}  Precision: {:.2%}'''.format(rec, prec))
            print('''Number of predicted species {}'''.format(len(np.unique(y_pred))))

        return {'Recall': rec, 'Precision': prec}


    def predict_eval(self, model_path, input_data, input_labels, output_path):
        '''
        Make predictions and evaluate them
        '''
        time = self.make_predictions(model_path, input_data, output_path)
        results = self.evaluate_predictions(output_path, input_labels)
        results['Time'] = time
        return results

