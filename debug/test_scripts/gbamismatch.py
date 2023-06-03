output1 = open("mgbalog.txt", "r")
output2 = open("megalog.txt", "r")

s1 = output1.readlines()
s2 = output2.readlines()
output1.close()
output1.close()

maxLine = len(s1) > len(s2) and len(s2) or len(s1)

for line in range(0, maxLine):
    l1 = s1[line][:170]
    l2 = s2[line][:170]

    if l1 != l2:
        print(f"Mismatch Occured at Line {line+1}:")
        print(f"MGBA:     {s1[line-1]}\n\n{s1[line]}")
        print("---------------------------------")
        print(f"MegaGBA:  {s2[line-1]}\n\n{s2[line]}")
        c = input("Continue? (y/n): ")
        if c != "y": exit(0)

print("No Mismatches Found :D")
