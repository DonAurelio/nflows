import pathlib
from wfcommons import Instance
input_instance = pathlib.Path('montage-chameleon-2mass-005d-001.json')
instance = Instance(input_instance=input_instance)
instance.write_dot(pathlib.Path('sample-montage-workflow.dot'))
