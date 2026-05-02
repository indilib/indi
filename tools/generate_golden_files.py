"""
Generate golden reference data for libastro coordinate tests.

Queries IMCCE Miriade (INPOP19 ephemeris) for apparent horizontal coordinates
of Vega and Polaris at four IAU observatory sites, writing the results to
test/data/horizontal_golden.json.  That file is the external truth source for
the HorizontalAccuracy_vs_IMCCE test in test/core/test_libastro.cpp.

Usage:
    python3 tools/generate_golden_files.py

Requires: requests  (pip install requests)
"""

import requests
import sys
import json
import time

def fetch_imcce_horizontal(hip_id, name, ra_j2000_h, dec_j2000_deg, jd, iau_code, site_name):
    """Fetch horizontal (az/alt) truth from IMCCE Miriade for a star at a specific IAU site.

    Returns a record with the IMCCE-reported topocenter coordinates (lon/lat/elev) alongside
    the az/alt truth so the test can feed the exact same observer position to
    EquatorialToHorizontal.
    """
    url = "https://ssp.imcce.fr/webservices/miriade/api/ephemcc.php"
    params = {
        "-name": hip_id,
        "-type": "star",
        "-ep": jd,
        "-observer": iau_code,
        "-teph": "2",   # apparent of date
        "-tcoor": "5",  # horizontal coordinates (az/alt)
        "-mime": "json"
    }
    response = requests.get(url, params=params)
    data = response.json()
    if "data" not in data:
        print(f"  WARNING: no data for {name} at {site_name}: {data}", file=sys.stderr)
        return None
    row = data["data"][0]

    # Extract the topocenter coordinates IMCCE actually used so the test can reproduce
    # the exact same observer position.
    topo = data.get("ephemeris", {}).get("reference_frame", {}).get("topocenter", [{}])[0]
    lon = topo.get("longitude", 0.0)
    lat = topo.get("latitude", 0.0)
    elev = topo.get("altitude", 0.0)

    return {
        "label": f"{iau_code}_{name.lower()}",
        "object": name,
        "hip_id": int(hip_id),
        "ra_j2000_h": ra_j2000_h,
        "dec_j2000_deg": dec_j2000_deg,
        "jd": jd,
        "site": site_name,
        "iau_code": iau_code,
        "lon_deg": lon,
        "lat_deg": lat,
        "elev_m": elev,
        "az_deg": float(row["Az"]),
        "alt_deg": float(row["H"]),
        "source": "IMCCE Miriade INPOP19"
    }


if __name__ == "__main__":
    # JD 2459580.5 = 2022-Jan-01 00:00:00 UTC
    horiz_jd = 2459580.5

    # Hipparcos J2000 catalog coordinates
    stars_horiz = {
        # HIP ID: (name, ra_j2000_h, dec_j2000_deg)
        "91262":  ("Vega",    18.61564232, 38.78368900),  # HIP 91262
        "11767":  ("Polaris",  2.53030388, 89.26410897),  # HIP 11767
    }

    # IAU observatory codes to query.
    # Three sites spanning ~300 deg of longitude prove observer longitude is used.
    # Pairing Greenwich (lat 51.5 N) with Siding Spring (lat 31.3 S) proves observer
    # latitude is used — Polaris is circumpolar from Greenwich and below the horizon
    # from Siding Spring.
    sites = {
        "000": "Greenwich",
        "381": "Mitaka (Tokyo)",
        "568": "Mauna Kea",
        "413": "Siding Spring",
    }

    horiz_data = []
    print(f"Generating horizontal golden dataset for JD {horiz_jd}...")
    for hip_id, (name, ra_h, dec_deg) in stars_horiz.items():
        for iau_code, site_name in sites.items():
            print(f"  Fetching {name} at {site_name}...")
            res = fetch_imcce_horizontal(hip_id, name, ra_h, dec_deg, horiz_jd,
                                         iau_code, site_name)
            if res:
                horiz_data.append(res)
            time.sleep(0.5)

    with open("test/data/horizontal_golden.json", "w") as f:
        json.dump(horiz_data, f, indent=2)

    print("\nSuccess! test/data/horizontal_golden.json written.")
    for entry in horiz_data:
        print(f"  {entry['object']:<8} {entry['site']:<20}: Az={entry['az_deg']:9.4f}  Alt={entry['alt_deg']:9.4f}")
