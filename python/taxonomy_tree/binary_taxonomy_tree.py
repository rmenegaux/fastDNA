import numpy as np

from taxonomy_tree import Node


class BinaryNode():
    def __init__(self, left=None, right=None, parent=None, data=None):
        self.left = left
        self.right = right
        self.parent = parent
        self.data = data
        
    def __str__(self):
        print_binary_tree(self)
        return repr(self)
    
    def save(self, file_name):
        save_binary_tree(self, file_name)


def to_binary_(tree, node_counts):
    '''
    node_counts = {'leaf': 0, 'node': 0}
    '''
    n_children = len(tree.children)

    if n_children == 0:
        data = {
            'count': tree.data['count'],
            'taxids': [tree.data['taxid']],
            'id': 'l{}'.format(node_counts['leaf'])
        }
        node_counts['leaf'] += 1
        return BinaryNode(data=data)
    
    elif n_children == 1:
        # Skip the node and go directly to its child
        return to_binary_(next(iter(tree.children.values())), node_counts)
    
    else:
        data = merge_nodes_data([tree])
        data['id'] = 'n{}'.format(node_counts['node'])
        node_counts['node'] += 1
    
    binary_node = BinaryNode(data=data)
    left_right = []
    for nodes in split_nodes_evenly(tree.children):
        new_node = Node(
            children=nodes,
            data=merge_nodes_data(nodes.values())
        )
        new_node = to_binary_(new_node, node_counts)
        new_node.parent = binary_node
        left_right.append(new_node)    
    binary_node.left = left_right[0]
    binary_node.right = left_right[1]
    
    return binary_node

def print_binary_tree(node, level=0):
    '''
    Derived from https://stackoverflow.com/questions/34012886/
    '''
    if node != None:
        print_binary_tree(node.left, level + 1)
        print(' ' * 4 * level + '->', node.data['id'], node.data['taxids'], node.data['count'])
        print_binary_tree(node.right, level + 1)

def to_binary(tree):
    binary_tree = to_binary_(tree, {'leaf': 0, 'node': 0})
    # Make sure root has the total count
    binary_tree.data['count'] = 0
    for child in [binary_tree.left, binary_tree.right]:
        if child is not None:
            binary_tree.data['count'] += child.data['count']
    return binary_tree

def merge_nodes_data(nodes_list):
    taxids = []
    count = 0
    for node in nodes_list:
        new_taxids = node.data['taxids'] if 'taxids' in node.data else [node.data['taxid']]
        taxids += new_taxids
        count += node.data['count']
    return {'taxids': taxids, 'count': count}
    
def split_evenly(counts):
    sorted_args = np.argsort(counts)
    sorted_counts = np.array(counts)[sorted_args].cumsum()
    total_count = sorted_counts[-1]
    split_index = np.argmin(sorted_counts <= total_count / 2)
    return sorted_args[:split_index], sorted_args[split_index:]

def split_nodes_evenly(nodes_dict):
    tuples = np.array(list(zip(nodes_dict.keys(), nodes_dict.values())))
    counts = [t[1].data['count'] for t in tuples]
    left_indices, right_indices = split_evenly(counts)
    return dict(tuples[left_indices]), dict(tuples[right_indices])

def check_split():
    counts = np.array(list(range(15))+[7*12])[::-1]
    print(counts)
    left, right = split_evenly(counts)
    print(counts[left], counts[right])
    print('left sum {} | right sum {}'.format(counts[left].sum(), counts[right].sum()))
    
def save_binary_tree(tree, file_name):
    '''
    Format
    ------
    id_root count_root
    id_node1 id_parent1 count1 taxids1
    ...
    id_nodeN id_parentN countN taxidsN
    '''
    with open(file_name, 'w') as f:
        save_binary_tree_stream(tree, f)
        
def add_word(file_stream, word):
    file_stream.write(' {}'.format(word))

def save_binary_tree_stream(tree, file_stream):
    if tree is None:
        return
    file_stream.write(tree.data['id'])
    if tree.parent is not None:
        add_word(file_stream, tree.parent.data['id'][1:])
    add_word(file_stream, tree.data['count'])
    for taxid in tree.data['taxids']:
         add_word(file_stream, taxid)
    file_stream.write('\n')
    for child in [tree.left, tree.right]:
        save_binary_tree_stream(child, file_stream)