experiment_name="test"
output_dir="output/cami/${experiment_name}"
convert_to_cami="cami_evaluation/to_biobox.sh"
sample_name="large"

data_cami=/mnt/data40T/rmenegaux/these_romain/data/CAMI_medium
tax_dir=$data_cami/taxonomy

while getopts ids:truth: flag
do
    case "${flag}" in
        ids) seqids=${OPTARG};;
        truth) gold_standard=${OPTARG};;
    esac
done

local i=0 predictions_cami_format=()
    for prediction in "$@"
    do
        prediction_cami_format=$output_dir/$(basename $prediction).cami
        $convert_to_cami -s $seqids -o $prediction_cami_format -t $prediction -n $sample_name
        predictions_cami_format[$i]=$prediction_cami_format
        ((++i))
    done

command="amber.py ${predictions_cami_format[@]} --gold_standard_file $gold_standard --ncbi_nodes_file $tax_dir/nodes.dmp --ncbi_names_file $tax_dir/names.dmp --ncbi_merged_file $tax_dir/merged.dmp --filter 1 --output_dir $output"
echo $command
$command