outputOld = open("log2.txt", "r")
outputNew = open("log1.txt", "r")

s1 = outputOld.read()
s2 = outputNew.read()
outputOld.close()
outputNew.close()

line = 1
charcount = 1
for i in range(0, len(s1)):
        char = s1[i]
        if char == '\n':
            line += 1
            charcount = 0
        
        charcount += 1
        if s2[i] != char:
            print("Mismatch at line : " + str(line), "Char : " + str(charcount), "S1 : " + char, "S2 : " + s2[i])
            break


