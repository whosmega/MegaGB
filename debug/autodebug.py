# This program auto debugs the CPU instructions of megagbc by comparing the 
# instruction trace log of a modified version of binjgb emulator

import subprocess 
import sys

binjgbOutput = open("binjgbLog.txt", "w")
megagbcOutput = open("megagbcLog.txt", "w")

timeout = 3

try:
    subprocess.call(["./binjgb-debugger", sys.argv[1]], 
                    stdout = binjgbOutput, stderr = binjgbOutput, timeout = timeout)
except subprocess.TimeoutExpired:
    print("Logged binjgb trace")

try:
    subprocess.call(["../megagbc", sys.argv[1]],
                    stdout = megagbcOutput, stderr = megagbcOutput, timeout = timeout)
except subprocess.TimeoutExpired:
    print("Logged megagbc trace")

binjgbOutput.close()
megagbcOutput.close()

binjgbOutput = open("binjgbLog.txt", "r")
megagbcOutput = open("megagbcLog.txt", "r")

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
    l1 = line[:30]
    l2 = mLines[i][:30]

    if (l1 != l2):
        # mismatch occured
        print(f"Mismatch at line {i + 1}")
        print("===== Binjgb Log =====")
        printAround(bLines, i, bLinesLen)    
        print("======================\n")
        print("===== MegaGBC Log =====")
        printAround(mLines, i, mLinesLen)
        print("======================")
        break 

print("Finished")
    


