#!/bin/bash
#SBATCH --job-name=kallisto
#SBATCH --mem=120G
#SBATCH --output=/mnt/data40T/rmenegaux/fastkallisto/test/output/output_cami_15.log
#SBATCH --error=/mnt/data40T/rmenegaux/fastkallisto/test/output/error_cami_15.log

source /cbio/donnees/rmenegaux/.bashrc
# sleep 5m
sh /mnt/data40T/rmenegaux/fastkallisto/test/kallisto_index.sh
# SBATCH --nodelist=node24
# SBATCH --partition=gpu-cbio
