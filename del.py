import urllib.request
import json

# Certspotter API endpoint for nmap.org
url = "https://api.certspotter.com/v1/issuances?domain=nmap.org&include_subdomains=true&expand=issuer"
req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})

try:
    with urllib.request.urlopen(req) as response:
        certs = json.loads(response.read().decode())
        
        emudhra_certs = []
        for c in certs:
            issuer = c.get("issuer", {})
            issuer_name = issuer.get("name", "").lower()
            
            if "emudhra" in issuer_name or "emsign" in issuer_name:
                emudhra_certs.append(c)
        
        if emudhra_certs:
            print(f"Found {len(emudhra_certs)} eMudhra certificate(s) for nmap.org:")
            for c in emudhra_certs:
                print(f"Issued: {c.get('not_before')} | Issuer: {c.get('issuer', {}).get('name')}")
        else:
            print("No eMudhra/emSign certificates were found in Certspotter logs for nmap.org.")
            print("\nMost recent issuers found:")
            for c in certs[:5]:
                print(f"- {c.get('issuer', {}).get('name')}")
                
except Exception as e:
    print("Error querying Certspotter:", e)
