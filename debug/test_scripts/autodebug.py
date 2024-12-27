# This program auto debugs the CPU instructions of megagbc by comparing the 
# instruction trace log of a modified version of binjgb emulator

import subprocess 
import sys

binjgbOutput = open("log2.txt", "w")
megagbcOutput = open("log1.txt", "w")

timeout = 5
MaxCharPerLine = 20

try:
    subprocess.call(["./../binjgb/bin/binjgb", sys.argv[1]], 
                    stdout = binjgbOutput, stderr = binjgbOutput, timeout = timeout)
except subprocess.TimeoutExpired:
    print("Logged binjgb trace")

try:
    subprocess.call(["./megagb", sys.argv[1]],
                    stdout = megagbcOutput, stderr = megagbcOutput, timeout = timeout)
except subprocess.TimeoutExpired:
    print("Logged megagbc trace")

binjgbOutput.close()
megagbcOutput.close()

binjgbOutput = open("log2.txt", "r")
megagbcOutput = open("log1.txt", "r")

bLines = binjgbOutput.readlines()
mLines = megagbcOutput.readlines()

binjgbOutput.close()
megagbcOutput.close()

mLinesLen = len(mLines)
bLinesLen = len(bLines)

def printAround(array, i, maxLen):
    for ii in range(i - 5, i):
        if ii < 0:
            break

        print(f"    {array[ii][:-1]}")

    for ii in range(i, i + 6):
        if ii > maxLen - 1:
            break
        
        print(f"{ii == i and '>>> ' or '    '}{array[ii][:-1]}")

for i in range(0, bLinesLen):
    if i == mLinesLen - 1:
        break

    line = bLines[i]
    l1 = line[:MaxCharPerLine]
    l2 = mLines[i][:MaxCharPerLine]

    if (l1 != l2):
        # mismatch occured
        print(f"Mismatch at line {i + 1}")
        print("===== Binjgb Log =====")
        printAround(bLines, i, bLinesLen)    
        print("======================\n")
        print("===== MegaGBC Log =====")
        printAround(mLines, i, mLinesLen)
        print("======================")
        if input("Continue? (y/n)") == "n":
            break

print("Finished")
    


