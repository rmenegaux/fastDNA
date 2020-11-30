#convert to biobox format

prediction_name="fdna_cami_k14_d50_e100_lr0.1_n4.0_fragments.pred"
taxids="/mnt/data40T/rmenegaux/these_romain/fasttext/output/predictions/${prediction_name}"
#taxids=/mnt/data40T/rmenegaux/these_romain/fasttext/output/predictions/fdna_large_k13_d10_e50_lr0.1_fragments.pred
#taxids="/mnt/data40T/rmenegaux/these_romain/fastdna_tree/output/predictions/fdna_dmicrobes_k14_d50_e100_lr1_lhs_n4.pred"
#taxids="/mnt/data40T/rmenegaux/these_romain/fastdna_tree/output/predictions/fdna_dmicrobes_k14_d50_e1000_lr0.1_lhs_n4_tax.pred"
sample_name=gs
version=0.9.1
output="/mnt/data40T/rmenegaux/these_romain/cami_evaluation/biobox_files/${sample_name}_${prediction_name}.binning"
seqids="/mnt/data40T/rmenegaux/these_romain/data/CAMI_medium/seqids.500000.txt"

while getopts s:t:o: flag
do
    case "${flag}" in
        s) seqids=${OPTARG};;
        t) taxids=${OPTARG};;
        o) output=${OPTARG};;
    esac
done
seq 1 $(wc -l <$taxids) > seqids_tmp
seqids="${seqids:-seqids_tmp}"
# Stack seqids and taxids
paste $seqids $taxids > $output
rm seqids_tmp
# Add headers
sed -i 's/$/\t100/' $output
sed -i '1s/^/@@SEQUENCEID\tTAXID\t_LENGTH\n/' $output
sed -i '1s/^/\n/' $output
sed -i "1s/^/@SampleID:${sample_name}\n/" $output
sed -i "1s/^/@Version:${version}\n/" $output

# ------------------------
data_cami=/mnt/data40T/rmenegaux/these_romain/data/CAMI_medium
gold_standard=$data_cami/ground_truth.500000
predictions=biobox_files/gs_dm_training_set.binning
tax_dir=$data_cami/taxonomy
output=output/output_dmicrobes
command="amber.py $predictions --gold_standard_file $gold_standard --ncbi_nodes_file $tax_dir/nodes.dmp --ncbi_names_file $tax_dir/names.dmp --ncbi_merged_file $tax_dir/merged.dmp --filter 1 --output_dir $output"
echo $command
$command
