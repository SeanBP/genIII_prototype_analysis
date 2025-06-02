import pandas as pd
import awkward as ak

def datTXT_to_DF(path, max_events=None):
    with open(path) as f:
        lines = f.read().split('\n')

    data = []
    tags = None
    event_count = 0
    collecting = True

    for line in lines:
        if line.strip() == "" or line.startswith("//"):
            continue

        split = line.split()

        # Count events by detecting lines with 7 items
        if len(split) == 7:
            if max_events is not None and event_count >= max_events:
                collecting = False
                break
            event_count += 1
            #continue  # This is just a separator/indicator line, skip it

        if not collecting or len(split) < 4:
            continue  # skip malformed lines or if we're done collecting

        # First non-comment, valid line sets column headers
        if tags is None:
            tags = split[-3:]  # e.g., LG, HG, etc.
            continue

        CAEN, CAEN_ch, LG, HG = split[:4]
        channel = int(CAEN) * 64 + int(CAEN_ch)

        data.append({
            "CAEN": int(CAEN),
            "CAEN_ch": int(CAEN_ch),
            "channel": channel,
            "LG": float(LG),
            "HG": float(HG)
        })

    return pd.DataFrame(data)


def datROOT_to_DF(events):
    
    # Extract all channel keys
    channel_keys = [key for key in events.keys() if key.startswith("ch_")]

    # Load all channels into an awkward array dict
    channels = {key: events[key].array(library="ak") for key in channel_keys}

    # Create a list of awkward arrays for each channel with extra fields
    channel_arrays = []
    for key, arr in channels.items():
        channel = int(key.split("_")[1])
        arr_with_fields = ak.zip({
            "event": ak.local_index(arr),
            "channel": channel,
            "HG": arr["HG"],
            "LG": arr["LG"],
        })
        channel_arrays.append(arr_with_fields)

    # Concatenate all channel arrays
    all_data = ak.concatenate(channel_arrays)

    # Convert to pandas DataFrame
    df = ak.to_dataframe(all_data).reset_index(drop=True)
    df = df.sort_values(by=["event", "channel"]).reset_index(drop=True)
    df["CAEN_brd"] = df["channel"] // 64
    df["CAEN_ch"] = df["channel"] % 64
    
    return df

def simROOT_to_DF(events, mip_value=0.000875127161931984):
    # Load jagged arrays from branches
    energy = events["HcalFarForwardZDCHits/HcalFarForwardZDCHits.energy"].array()
    x = events["HcalFarForwardZDCHits/HcalFarForwardZDCHits.position.x"].array()
    y = events["HcalFarForwardZDCHits/HcalFarForwardZDCHits.position.y"].array()
    z = events["HcalFarForwardZDCHits/HcalFarForwardZDCHits.position.z"].array()

    # Add event number as a jagged array matching hit counts
    event_nums = ak.local_index(energy)

    # Normalize z so minimum is 0
    z = z - np.min(ak.to_numpy(ak.flatten(z)))

    # Combine into a jagged array of records
    hits = ak.zip({
        "event": event_nums,
        "energy_GeV": energy,
        "x": x,
        "y": y,
        "z": z
    })

    # Flatten to get one row per hit
    df = ak.to_dataframe(ak.flatten(hits, axis=0)).reset_index(drop=True)

    # Build full grid of all (event, x, y, z) combinations
    unique_xyz = df[['x', 'y', 'z']].drop_duplicates()
    unique_events = df['event'].drop_duplicates()

    full_grid = pd.merge(
        unique_events.to_frame(name='event').assign(dummy=1),
        unique_xyz.assign(dummy=1),
        on='dummy'
    ).drop(columns='dummy')

    # Merge original hits into full grid
    full_df = pd.merge(
        full_grid,
        df,
        on=['event', 'x', 'y', 'z'],
        how='left'
    )

    # Fill missing energies with 0 and sort
    full_df['energy_GeV'] = full_df['energy_GeV'].fillna(0).astype(float)
    full_df = full_df.sort_values(by=["event", "z", "y", "x"]).reset_index(drop=True)

    # Assign layer indices by z value
    z_to_layer = {z: i for i, z in enumerate(sorted(full_df['z'].unique()))}
    full_df['layer'] = full_df['z'].map(z_to_layer)

    # Re-sort before assigning layer channel index
    full_df = full_df.sort_values(by=["event", "layer", "y", "x"]).reset_index(drop=True)
    full_df['layer_ch'] = full_df.groupby(['event', 'layer']).cumcount()

    # Add energy in MIP units
    full_df["energy_MIP"] = full_df["energy_GeV"] / mip_value

    return full_df

def apply_pedestal_corrections(df: pd.DataFrame, ped_df: pd.DataFrame, threshold: float = 3) -> pd.DataFrame:
    # Ensure 'channel' exists in df based on CAEN and CAEN_ch
    if 'channel' not in df.columns and {'CAEN', 'CAEN_ch'}.issubset(df.columns):
        df = df.copy()
        df["channel"] = df["CAEN"] * 64 + df["CAEN_ch"]

    # Drop duplicate columns from pedestal dataframe to avoid _x/_y confusion
    ped_df_clean = ped_df.drop(columns=["CAEN", "CAEN_ch"], errors="ignore")

    # Merge pedestal info
    merged = pd.merge(df, ped_df_clean, on="channel", how="left")

    # Compute pedestal-corrected values
    merged["HG_ped_corr"] = merged["HG"] - merged["HGPedMean"]
    merged["LG_ped_corr"] = merged["LG"] - merged["LGPedMean"]

    # Apply threshold cut
    merged["HG_ped_corr"] = merged["HG_ped_corr"].where(
        merged["HG_ped_corr"] >= threshold * merged["HGPedSigma"], 0
    )
    merged["LG_ped_corr"] = merged["LG_ped_corr"].where(
        merged["LG_ped_corr"] >= threshold * merged["LGPedSigma"], 0
    )

    # Drop the pedestal mean and sigma columns before returning
    columns_to_drop = ["HGPedMean", "HGPedSigma", "LGPedMean", "LGPedSigma"]
    merged = merged.drop(columns=columns_to_drop, errors="ignore")

    return merged



def apply_geometry(df: pd.DataFrame, geo_df: pd.DataFrame) -> pd.DataFrame:
    # Merge geometry information
    merged = pd.merge(df, geo_df[["channel", "x", "y", "z", "layer", "layer_ch"]], on="channel", how="left")
    
    return merged