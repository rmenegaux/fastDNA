import numpy as np

class Node():
    def __init__(self, children=None, parent=None, data=None):
        if children is None:
            self.children = {}
        else:
            self.children = children
        self.parent = parent
        self.data = data
        
    def __str__(self):
        string_rep = 'Node('
        for child in self.children.values():
            string_rep += str(child)
        string_rep += ')'
        return string_rep
    
    def __repr__(self):
        return '''
Node data: {}
Children: {}
'''.format(self.data, list(self.children.keys()))
    
    def get_children(self):
        return list(self.children.keys())

def print_tree(node, level=0):
    '''
    Derived from https://stackoverflow.com/questions/34012886/
    '''
    if node != None:
        children_ids = list(node.children)
        mid_child = len(children_ids) // 2
        for child in children_ids[:mid_child]:
            print_tree(node.children[child], level + 1)
        print(' ' * 4 * level + '->', node.data['id'], node.data['taxids'], node.data['count'])
        for child in children_ids[mid_child:]:
            print_tree(node.children[child], level + 1)

def tree_from_taxid_file(taxid_file, reference_tree):
    taxid_list = np.loadtxt(taxid_file, dtype=int)
    return tree_from_taxids(taxid_list, reference_tree)

def tree_from_taxids(taxid_list, reference_tree):
    unique, counts = np.unique(taxid_list, return_counts=True)
    taxid_counts = dict(zip(unique, counts))
    tree = Node(data={'taxid': 1, 'count': 0, 'rank': 'root'})
    for taxid, count in taxid_counts.items():
        add_taxid(tree, taxid, reference_tree, count)
    assert(tree.data['count'] == np.sum(counts))
    return tree
    
def add_taxid(tree, taxid, reference_tree, count=1):
    from load_ncbi import get_id_path_

    path = get_id_path_(taxid, reference_tree)
    if '' in path:
        print('Warning: empty level for taxid {}, lineage is {}'.format(taxid, path))

    tree.data['count'] += count
    current_node = tree
    for ancestor_taxid in path:
        if ancestor_taxid == '':
            # Investigation needed here as to why some taxids come up empty
            continue
        if ancestor_taxid in reference_tree.tax_id_to_tax_id:
            primary_taxid = reference_tree.tax_id_to_tax_id[ancestor_taxid]
            print('Warning: taxid {} is mapped to primary taxid {}'.format(ancestor_taxid, primary_taxid))
        else:
            primary_taxid = ancestor_taxid
        if primary_taxid in current_node.children:
            current_node.children[primary_taxid].data['count'] += count
        else:
            data = {'taxid': ancestor_taxid, 'count': count}
            new_node = Node(
                parent=current_node,
                data={
                    'taxid': ancestor_taxid,
                    'count': count,
                    'rank': reference_tree.tax_id_to_rank.get(primary_taxid, 'no rank')
                    })
            current_node.children[primary_taxid] = new_node
        current_node = current_node.children[primary_taxid]

