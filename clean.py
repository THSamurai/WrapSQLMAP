#!/usr/bin/env python
#coding: utf-8
from sys import argv
from urlparse import urlparse


if __name__ == '__main__':
    domains = []
    dirty = open(argv[1]).read().splitlines()
    clean = open(argv[1].replace('.txt', '_clean.txt'), 'w')
    for line in dirty:
        try:
            modif_line = line.decode('utf8')
        except: pass
        else:
            try:
                domain = urlparse(modif_line).netloc.replace('www.', '')
                if '=' and '?' in line:
                    if domain not in domains:
                        domains.append(domain)
                        clean.write('%s\n' % line)
                        #print 'Write url: %s' % line
                    else:
                        pass
                        #print 'Find dublicate:%s' % line
                else:
                    pass
                    #print 'Bad url: %s' % line
            except: pass
    clean.close()

