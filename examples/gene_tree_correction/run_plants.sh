generax=build/bin/generax
example=examples/gene_tree_correction
output=$example/output
families=$example/families_plants.txt
speciestree=data/plants/species_trees/speciesTree.newick
cores=2

if [ ! -f "$generax" ]; then
  echo "Can't find executable $generax. Please check that you compiled generax and that you ran this script from the repository root directory."
  exit 1
fi

rm -rf $output

mpirun -np $cores $generax --families $families --species-tree $speciestree --rec-model UndatedDL --per-family-rates  --prefix $output --max-spr-radius 3

