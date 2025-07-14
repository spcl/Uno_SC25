# Takes in a whole command, runs its and then writes the command ran into folder/cmd.txt

import sys
import os

# Get the folder.
folder = sys.argv[-1]


cmd = " ".join(sys.argv[1:])

# Write the command ran into folder/cmd.txt
with open(f"{folder}/cmd.txt", "w") as f:
    f.write(cmd)
    f.write('\n')

# Run the command
os.system(cmd)