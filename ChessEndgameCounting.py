def F(n):
    p = 1
    for i in range(1,n+1):
        p *= i
    return p

def C(n,r):
    return F(n)/F(n-r)/F(r)

def P(n,r):
    return F(n)/F(n-r)

def CR(n,r):
    return C(n+r-1,r)

def main():
    print "Lomonosov Endgame Tablebases use 140 terrabytes"
    print " 140000000000000"
    c1  = CR(5,3) * CR(5,2)
    c2 = CR(5,4) * CR(5,1)
    ct = c1+c2
    print "Number of tables is",ct
    print "Bytes per table is",140000000000000/ct
    print "which is 160 billion, compared to my 4.158 trillion"

    n=5
    boardPositions = 64**2 * 65**n
    print "\nMy board position count is:"
    print "  ",boardPositions
    print "Requiring this byte count:"
    print boardPositions * ct
    


main()
