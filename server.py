import http.server
import socketserver
import os
import sys
import urllib.parse

PORT = 8000
DIRECTORY = os.path.dirname(os.path.abspath(__file__))

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def do_GET(self):
        if self.path in ('/setup', '/setup/'):
            self.path = '/setup.html'
        super().do_GET()

    def do_POST(self):
        if self.path == '/save':
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length).decode('utf-8')
            fields = urllib.parse.parse_qs(post_data)
            
            ssid = fields.get('ssid', [''])[0]
            lat = fields.get('lat', [''])[0]
            lon = fields.get('lon', [''])[0]
            print(f"[MOMO CONFIG SAVED] SSID: {ssid}, Lat: {lat}, Lon: {lon}")
            
            response = (
                "<html>"
                "<head><meta charset='UTF-8'></head>"
                "<body style='font-family:Arial;text-align:center;"
                "background:#111;color:white;padding:40px'>"
                "<h1>MOMO</h1>"
                "<p>Configuration saved.</p>"
                "<p>Momo is restarting...</p>"
                "<script>setTimeout(function(){ window.location.href = '/'; }, 2500);</script>"
                "</body>"
                "</html>"
            ).encode('utf-8')
            
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(response)))
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(response)
        else:
            self.send_error(404, "Not Found")

    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        super().end_headers()

def run_server():
    os.chdir(DIRECTORY)
    for p in [8000, 8080, 8888, 5000]:
        try:
            with socketserver.TCPServer(("", p), Handler) as httpd:
                print(f"MOMO_SERVER_STARTED: http://localhost:{p}")
                sys.stdout.flush()
                httpd.serve_forever()
        except OSError:
            continue

if __name__ == "__main__":
    run_server()
