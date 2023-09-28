#! /usr/bin/env python3

import sys
import os.path
from cryptography import x509
from asn1crypto import core

if len(sys.argv) < 2:
    print('Usage: {} <cert filepath>'.format(os.path.basename(sys.argv[0])))
    sys.exit(1)

trusted_ctr_oid = '1.3.6.1.4.1.4128.2100.1'
non_trusted_ctr_oid = '1.3.6.1.4.1.4128.2100.2'

def show_counter(name, oid, asn1value):
    ctr = core.Asn1Value.load(asn1value)
    print('{}: {}'.format(name, oid))
    print('Value: {} = {}'.format(ctr.__class__, ctr.native))

with open(sys.argv[1], 'rb') as fobj:
    data = fobj.read()
    cert = x509.load_der_x509_certificate(data)
    print(cert.subject.rfc4514_string())
    for ext in cert.extensions:
        if ext.oid.dotted_string == trusted_ctr_oid:
            show_counter('Trusted Counter', ext.oid.dotted_string, ext.value.value)
        if ext.oid.dotted_string == non_trusted_ctr_oid:
            show_counter('Non trusted Counter', ext.oid.dotted_string, ext.value.value)

