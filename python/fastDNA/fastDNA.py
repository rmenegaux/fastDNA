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
              length=200, epoch=1, lr=0.1, noise=0,
              loss='softmax',
              freeze=False, pretrained_vectors="", previous_model="",
              threads=4):
        '''
        Trains a fastdna model
        '''
        # Basic training command
        command = '{} supervised -input {} -output {} -labels {}'.format(
            self.fastdna, train_data, model_path, train_labels)

        # Parameters for training
        parameters = ' -epoch {} -dim {} -lr {} -minn {} -maxn {} -thread {} -length {} -loss {}'.format(
            epoch, dim, lr, k, k, threads, length, loss)
        if noise > 0:
            parameters += ' -noise {}'.format(int(noise * 1000))
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


    def make_predictions(self, model_path, input_data, output_path, paired=False, threshold=0.):
        ''' 
        Predicts the labels for `input_data`
        Stores the results in `output_path`
        '''
        predict = 'predict-paired' if paired else 'predict'
        # predict one label only for now
        command = '{} {} {} {} 1 {} > {}'.format(
            self.fastdna, predict, model_path, input_data, threshold, output_path)

        readout = 'Making predictions {}'.format(model_path.split('/')[-1])
        return timed_check_call(command, name=readout, verbose=self.verbose)


    def predict_eval(self, model_path, input_data, input_labels, output_path, paired=False, threshold=0.):
        '''
        Make predictions and evaluate them
        '''
        from evaluate import evaluate_predictions

        time = self.make_predictions(
            model_path, input_data, output_path, paired=paired, threshold=threshold
            )
        results = evaluate_predictions(output_path, input_labels, verbose=self.verbose)
        results['Time'] = time
        return results

