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
        str_ = str(self)
        for child in self.children.values():
            repr_ += str(child)
        return str_
    
    def __repr__(self):
        return '''
Node data: {}
Children: {}
'''.format(self.data, list(self.children.keys()))
    
    def get_children(self):
        return list(self.children.keys())


def tree_from_taxid_file(taxid_file, reference_tree):
    taxid_list = np.loadtxt(taxid_file, dtype=int)
    return tree_from_taxids(taxid_list, reference_tree)

def tree_from_taxids(taxid_list, reference_tree):
    unique, counts = np.unique(taxid_list, return_counts=True)
    taxid_counts = dict(zip(unique, counts))
    tree = Node(data={'taxid': 1, 'count': 0})
    for taxid, count in taxid_counts.items():
        add_taxid(tree, taxid, reference_tree, count)
    return tree
    
def add_taxid(tree, taxid, reference_tree, count=1):
    from load_ncbi import get_id_path_

    path = get_id_path_(taxid, reference_tree)
    current_node = tree
    for ancestor_taxid in path:
        if ancestor_taxid in current_node.children:
            current_node.children[ancestor_taxid].data['count'] += count
        else:
            new_node = Node(parent=current_node, data={'taxid': ancestor_taxid, 'count': count})
            current_node.children[ancestor_taxid] = new_node
        current_node = current_node.children[ancestor_taxid]
    return current_node

