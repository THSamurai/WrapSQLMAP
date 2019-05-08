#coding: utf-8

# proxy types: http|https|socks4|socks5


DEBUG = True # False - without logs
#DEBUG = False # False - without logs
LOG_FILE = 'wrapper_session.log'

SQLMAP_DUMPS = 'dumps'
WRAPPER_TXT_DUMPS = 'txt_dumps'

URLS_FILE = 'sites.txt'
SQLi_SAVE_FILE = 'goodsqli.txt'
BRUTE_SAVE_FILE = 'goodbrute.txt'
Check_SQLi = True
DUMP = False
PASSW_FILE = 'passw.txt'
COLUMN_DUMP = 'mail,pass,pwd,phone,ssn'
Check_List = True
BRUTE_CMS = True
PROXY = False # False if work without it
TAMPER = 'space2plus'
PROXY_TYPE = 'socks5'
PROXY_URL = '' # first
PROXY_FILE = '' # second if not first
PROXY_USERNAME = ''
PROXY_PASSWORD = ''
THREADS = 10
URLS_LIMIT = 10000000
DUMP_FOLDER = 'dump'
DUMP_COLUMN_LIMIT = 20 # 91 - 100 ...
TIMEOUT = 240 # sec
RETRIES = 5

DELETE=True

RISK = 3
LEVEL = 5
