import sys
import argparse


from taxonomy_tree import tree_from_taxid_file
from binary_taxonomy_tree import to_binary
from load_ncbi import load_ncbi_info, get_id_path_


def create_tree(taxid_file, nodes_file, names_file, merged_file, output_file=None):
    print('Loading NCBI info')
    ncbi_info = load_ncbi_info(nodes_file, names_file, merged_file)
    print('Creating taxonomy tree')
    tree = tree_from_taxid_file(taxid_file, reference_tree=ncbi_info)
    if output_file is None:
        output_file = 'fdna_tree.txt'
    print('Saving binary tree to {}'.format(output_file))
    to_binary(tree).save(output_file)

if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description="build a fdna tree from a list of taxids and a reference taxonomy tree"
        )
    parser.add_argument("taxids", help="list of taxids to tree-ify",
                        type=str)
    parser.add_argument("--output", help="name of output file",
                        type=str, default=None)
    # Download these from ftp://ftp.ncbi.nih.gov/pub/taxonomy/taxdump.tar.gz
    parser.add_argument("--ncbi_nodes_file", help="nodes.dmp from ncbi website",
                        type=str, required=True)
    parser.add_argument("--ncbi_names_file", help="names.dmp from ncbi website",
                        type=str, default=None)
    parser.add_argument("--ncbi_merged_file", help="merged.dmp from ncbi website",
                        type=str, default=None)
    args = parser.parse_args()

    create_tree(args.taxids, args.ncbi_nodes_file, args.ncbi_names_file,
                args.ncbi_merged_file, args.output)
