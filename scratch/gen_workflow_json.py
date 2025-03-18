import pathlib
from wfcommons.wfchef.recipes import MontageRecipe
from wfcommons.wfchef.recipes import EpigenomicsRecipe
from wfcommons import Instance
from wfcommons import WorkflowGenerator

generator = WorkflowGenerator(
    MontageRecipe(
        num_tasks=60, 
        data_footprint=64000000000, 
    )
)

workflow = generator.build_workflow()
workflow.write_dot(dot_file_path=pathlib.Path('montage-workflow.dot'))
