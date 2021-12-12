import machine
from machine import Pin
import time
import camera
import ubinascii
import socket



# (host, port) = ('13.235.33.131', 5000)

(host, port) = ('192.168.1.5', 15555)  # websocket conn


def _post_headers(host, port, url, ctype='json', size=5):
    header = ( b"POST {url} HTTP/1.1\r\nContent-Type: application/{ctype}\r\nContent-Length: {size}\r\nHost: {host}:{port}\r\nConnection: close\r\n" )

    header = header.format(host=host, port=port, url=url, ctype=ctype, size=size)

    #if encode:
    #    header = header.encode('iso-8859-1')

    return header


def send(host, port, payload, retry_count=3):

    socket_ = None
    for i in range(retry_count):
        try:
            socket_ = socket.socket()
            socket_.connect((host, port))
            socket_.send(payload)
            print('SENT payload')

        except Exception as e:
            print('Retry', i, e)
            time.sleep(1)
    if socket_ and hasattr(socket_, 'close'):
        socket_.close()


def post(url, data):
    global host, port

    data = ubinascii.b2a_base64(data)

    headers = _post_headers(host, port, url, size=len(data))
    payload = headers + "\r\n" + data

    send(host, port, payload)



def init_camera():
    import camera

    camera.init()
    camera.quality(10)  # initially it was 12, 6
    camera.framesize(8)  # initially it was 9, 4
    print("Camera initialized")


def wifi():
    import network

    wlan = network.WLAN(network.STA_IF) # create station interface
    wlan.active(True)       # activate the interface
    wlan.scan()             # scan for access points
    print('Is connected to wifi? ', wlan.isconnected())      # check if the station is connected to an AP
    wlan.connect('NETGEAR35', 'silentvase911') # connect to an AP
    wlan.config('mac')      # get the interface's MAC address
    print('IP Config is', wlan.ifconfig())         # get the interface's IP/netmask/gw/DNS addresses
    print('Is connected2 to wifi? ', wlan.isconnected())


time.sleep(4)

wifi()

time.sleep(4)

init_camera()

time.sleep(5)


def ws():
    #socket_ = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    socket_ = socket.socket()
    socket_.connect((host, port))
    for i in range(99000):
        try:
            data = camera.capture()
            print('.')
            data = ubinascii.b2a_base64(data)
            socket_.send(data)
            print(i, 'SENT image', len(data))
            time.sleep(0.2)
            #break

        except Exception as e:
            print('Retry', i, e)
            time.sleep(1)
        #if i == 9:
        #    socket_.send('close')

    if socket_ and hasattr(socket_, 'close'):
        print('closing socket')
        socket_.close()

ws()

motion = False

def handle_interrupt(pin):
     global motion
     motion = True
     global interrupt_pin
     interrupt_pin = pin


led = Pin(4, Pin.OUT)
# pir = Pin(14, Pin.IN)
#
# pir.irq(trigger=Pin.IRQ_RISING, handler=handle_interrupt)
#
# while True:
#     if motion:
#         print('Motion detected! Interrupt caused by:', interrupt_pin)
#         led.value(1)
#         buf = camera.capture()
#         post('/images', buf)
#         led.value(0)
#         print('Motion stopped!')
#         motion = False
#     else:
#         print('nooooo')
#         #led.value(1)
#         time.sleep(0.5)
#         #led.value(0)
#         #sleep(3)
#

