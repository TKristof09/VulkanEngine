
for filename in *.vert; do
    glslangValidator -V $filename
done
for filename in *.frag; do
    glslangValidator -V $filename
done
