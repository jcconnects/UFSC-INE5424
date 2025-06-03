import sys

def filter_helgrind_log(input_path, output_path, exclude_pattern="debug.h"):
    with open(input_path, "r") as infile, open(output_path, "w") as outfile:
        block = []
        skip_block = False
        for line in infile:
            block.append(line)
            if exclude_pattern in line:
                skip_block = True
            if line.strip().startswith("==") and "----------------------------------------------------------------" in line:
                # End of block
                if not skip_block:
                    outfile.writelines(block)
                block = []
                skip_block = False
        # Write any remaining block if not skipped
        if block and not skip_block:
            outfile.writelines(block)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 filter_helgrind_log.py <input_log> <output_log>")
        sys.exit(1)
    filter_helgrind_log(sys.argv[1], sys.argv[2])
