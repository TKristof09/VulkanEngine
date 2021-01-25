
for filename in *.vert; do
    glslangValidator -V $filename -o $filename.spv
done
for filename in *.frag; do
    glslangValidator -V $filename -o $filename.spv
done
