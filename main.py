# /usr/bin/python
# -*- coding: utf-8 -*-

import re
import glob
import asyncio
import os
from shutil import copyfile


visa = re.compile(b'4[0-9]{12}(?:[0-9]{3})?')
amex = re.compile(b'3[47][0-9]{13}')
bcglobal = re.compile(b'(6541|6556)[0-9]{12}')
cartleblance = re.compile(b'389[0-9]{11}')
dinnersclub = re.compile(b'3(?:0[0-5]|[68][0-9])[0-9]{11}')
discovercard = re.compile(b'65[4-9][0-9]{13}|'
                          b'64[4-9][0-9]{13}|'
                          b'6011[0-9]{12}|(622(?:12[6-9]|'
                          b'1[3-9][0-9]|[2-8][0-9][0-9]|'
                          b'9[01][0-9]|92[0-5])[0-9]{10})')
instapayment = re.compile(b'63[7-9][0-9]{13}')
jcbcard = re.compile(b'(?:2131|1800|35\d{3})\d{11}')
koreanlocalcard = re.compile(b'9[0-9]{15}')
lasercard = re.compile(b'(6304|6706|6709|6771)[0-9]{12,15}')
maestrocard = re.compile(b'(5018|5020|5038|6304|6759|6761|6763)[0-9]{8,15}')
mastercard = re.compile(b'5[1-5][0-9]{14}')
solocard = re.compile(b'(6334|6767)[0-9]{12}|(6334|6767)[0-9]{14}|(6334|6767)[0-9]{15}')
switchcard = re.compile(b'(4903|4905|4911|4936|6333|6759)[0-9]{12}'
                        b'|(4903|4905|4911|4936|6333|6759)[0-9]{14}'
                        b'|(4903|4905|4911|4936|6333|6759)[0-9]{15}|564182[0-9]{10}'
                        b'|564182[0-9]{12}|564182[0-9]{13}|633110[0-9]{10}'
                        b'|633110[0-9]{12}|633110[0-9]{13}')
unionpaycard = re.compile(b'(62[0-9]{14,17})')
ssn_1 = re.compile(b'\d{3}-\d{2}-\d{4}')
ssn_2 = re.compile(b'\d{9}')

def read_and_write(filepath):
    with open(filepath, 'rb') as f:
        text = f.read()

    if len(set(visa.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(visa.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(amex.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(amex.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(dinnersclub.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(dinnersclub.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(discovercard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(discovercard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(instapayment.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(instapayment.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(jcbcard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(jcbcard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(koreanlocalcard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(koreanlocalcard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(lasercard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(lasercard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(maestrocard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(maestrocard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(mastercard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(mastercard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(solocard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(solocard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(switchcard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(switchcard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(unionpaycard.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(unionpaycard.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(cartleblance.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.cc', 'a') as file:
            for line in set(cartleblance.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(ssn_1.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.ssn', 'a') as file:
            for line in set(ssn_1.findall(text)):
                file.write(line.decode('utf-8') + '\n')

    if len(set(ssn_2.findall(text))) != 0:
        with open('output/' + filepath.split('/')[1] + '.ssn', 'a') as file:
            for line in set(ssn_2.findall(text)):
                file.write(line.decode('utf-8') + '\n')


def read_dir(path):
    return glob.glob(path)


async def asynchronous():
    if not os.path.exists('output'):
        os.makedirs('output')

    for file in read_dir('input/*.txt'):
        copyfile("input/" + file.split('/')[1], 'output/' + file.split('/')[1])

    tasks = [asyncio.ensure_future(read_and_write(filepath) for filepath in read_dir('input/*.txt'))]
    await asyncio.wait(tasks)


def main():
    ioloop = asyncio.get_event_loop()
    ioloop.run_until_complete(asynchronous())
    ioloop.close()


if __name__ == '__main__':
    main()