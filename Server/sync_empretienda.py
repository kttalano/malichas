import sys
import json
import pandas as pd
import math
import unicodedata
import re

def sanitize_attr(val):
    if pd.isna(val): return ''
    val = str(val).strip().lower()
    if val in ['nan', 'none', 'null', 'undefined', 'unico', 'único', '-']: return ''
    val = ''.join(c for c in unicodedata.normalize('NFD', val) if unicodedata.category(c) != 'Mn')
    return val

def sync_empretienda(input_path, json_path, output_excel, output_html, margin_pct):
    with open(json_path, 'r', encoding='utf-8') as f:
        inventory = json.load(f)

    # 1. DICCIONARIO INTELIGENTE O(1)
    inv_dict = {}
    brands = set()
    
    for fam in inventory:
        if not isinstance(fam, dict): continue
        sku = str(fam.get('m_sku', '')).strip().lower()
        
        m_obj = fam.get('m_marca', '')
        marca = str(m_obj.get('m_name', m_obj.get('nombre', '')) if isinstance(m_obj, dict) else m_obj).strip().lower()
        
        if marca: brands.add(marca)
        if sku:
            inv_dict[sku] = fam
            if marca:
                # Guardamos combinaciones comunes por si el excel lo tiene todo junto
                inv_dict[f"{marca} {sku}"] = fam
                inv_dict[f"{sku} {marca}"] = fam
                inv_dict[f"{marca}-{sku}"] = fam

    brands_list = sorted(list(brands), key=len, reverse=True)

    try:
        df = pd.read_excel(input_path, dtype=str)
    except Exception:
        df = pd.read_csv(input_path, dtype=str)

    df = df.fillna('')
    logs = []
    processed_vars = set()

    for idx, row in df.iterrows():
        is_new_injected = str(row.get('__NEW__', '')) != ''
        raw_sku = str(row.get('SKU', '')).strip()
        sku_lower = raw_sku.lower()
        
        # 1. Match Directo o Combinado
        fam = inv_dict.get(sku_lower)
        
        # 2. Si tiene corchetes (ej: [Trenda] 137)
        if not fam and sku_lower.startswith('['):
            close_idx = sku_lower.find(']')
            if close_idx != -1:
                fam = inv_dict.get(sku_lower[close_idx+1:].strip())
                
        # 3. Strip de marcas (ej: "Trenda 137" -> "137")
        if not fam:
            for b in brands_list:
                if b and b in sku_lower:
                    clean_sku = re.sub(r'\b' + re.escape(b) + r'\b', '', sku_lower, flags=re.IGNORECASE).strip()
                    clean_sku = re.sub(r'[\s\-]+$', '', clean_sku).strip()
                    if clean_sku in inv_dict:
                        fam = inv_dict[clean_sku]
                        break

        old_stock = str(row.get('Stock', '0')).replace('.0', '')
        old_price = str(row.get('Precio', '0')).replace('.0', '')
        
        if fam:
            real_sku_json = str(fam.get('m_sku', '')).strip().lower()
            
            n1 = str(row.get('Nombre atributo 1', '')).lower()
            v1 = sanitize_attr(row.get('Valor atributo 1', ''))
            n2 = str(row.get('Nombre atributo 2', '')).lower()
            v2 = sanitize_attr(row.get('Valor atributo 2', ''))
            
            row_c, row_t = "", ""
            if 'color' in n1: row_c = v1
            if 'color' in n2: row_c = v2
            if 'talle' in n1: row_t = v1
            if 'talle' in n2: row_t = v2
            if not row_c and not row_t and v1: row_t = v1

            matched_var = None
            for v in fam.get('m_variantes', []):
                vt = sanitize_attr(v.get('m_talle', ''))
                vc = sanitize_attr(v.get('m_color', ''))
                if vt == row_t and vc == row_c:
                    matched_var = v
                    break
            
            if matched_var:
                stock_val = int(matched_var.get('m_stock', 0))
                p_esp = float(matched_var.get('m_precioEspecifico', 0) or 0)
                p_base = float(fam.get('m_precioBase', 0) or 0)
                p_raw = p_esp if p_esp > 0 else p_base
                
                new_price = int(math.ceil(p_raw * (1 + margin_pct/100) / 100) * 100)
                
                df.at[idx, 'Stock'] = stock_val
                df.at[idx, 'Mostrar en tienda'] = 'Si' if stock_val > 0 else 'No'
                df.at[idx, 'Precio'] = new_price
                
                processed_vars.add(f"{real_sku_json}|{row_t}|{row_c}")
                
                accion = "Nuevo Agregado" if is_new_injected else ("Actualizado" if stock_val > 0 else "Agotado (Stock 0)")
                logs.append({"sku": raw_sku, "nombre": row.get('Nombre',''), "variante": f"T: {row_t.title() or '-'} | C: {row_c.title() or '-'}", "accion": accion, "stock_old": old_stock, "stock_new": stock_val, "precio_old": old_price, "precio_new": new_price})
            else:
                df.at[idx, 'Stock'] = 0
                df.at[idx, 'Mostrar en tienda'] = 'No'
                logs.append({"sku": raw_sku, "nombre": row.get('Nombre',''), "variante": f"T: {row_t.title() or '-'} | C: {row_c.title() or '-'}", "accion": "Variante Obsoleta (Oculta)", "stock_old": old_stock, "stock_new": 0, "precio_old": old_price, "precio_new": "-"})
        else:
            df.at[idx, 'Stock'] = 0
            df.at[idx, 'Mostrar en tienda'] = 'No'
            logs.append({"sku": raw_sku, "nombre": row.get('Nombre',''), "variante": "-", "accion": "Producto Inexistente (Oculto)", "stock_old": old_stock, "stock_new": 0, "precio_old": old_price, "precio_new": "-"})

    # Buscar Faltantes
    for fam in inventory:
        if not isinstance(fam, dict): continue
        sku_key = str(fam.get('m_sku', '')).strip().lower()
        if not sku_key: continue
        
        for v in fam.get('m_variantes', []):
            vt = sanitize_attr(v.get('m_talle', ''))
            vc = sanitize_attr(v.get('m_color', ''))
            if f"{sku_key}|{vt}|{vc}" not in processed_vars:
                logs.append({"sku": fam.get('m_sku',''), "nombre": fam.get('m_nombre',''), "variante": f"T: {vt.title() or '-'} | C: {vc.title() or '-'}", "accion": "Faltante en Tienda", "stock_old": "-", "stock_new": v.get('m_stock',0), "precio_old": "-", "precio_new": "-"})

    if '__NEW__' in df.columns: df = df.drop(columns=['__NEW__'])
    df.to_excel(output_excel, index=False, engine='openpyxl')

    # HTML
    total_act = sum(1 for l in logs if l['accion'] == 'Actualizado')
    total_nuevos = sum(1 for l in logs if l['accion'] == 'Nuevo Agregado')
    total_agotados = sum(1 for l in logs if l['accion'] == 'Agotado (Stock 0)')
    total_obsoletas = sum(1 for l in logs if l['accion'] == 'Variante Obsoleta (Oculta)')
    total_inexistentes = sum(1 for l in logs if l['accion'] == 'Producto Inexistente (Oculto)')
    total_faltantes = sum(1 for l in logs if l['accion'] == 'Faltante en Tienda')

    html_content = f'''<!DOCTYPE html><html lang="es"><head><meta charset="UTF-8"><title>Reporte Empretienda</title>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css" rel="stylesheet">
    <style>
        :root {{ --primary: #3b82f6; --success: #10b981; --warning: #f59e0b; --danger: #ef4444; --gray: #94a3b8; --darkgray: #475569; --bg: #f8fafc; --text: #334155; --dark: #0f172a; }}
        body {{ font-family: 'Segoe UI', system-ui, sans-serif; background-color: var(--bg); color: var(--text); padding: 20px; }}
        .container {{ max-width: 1400px; margin: auto; background: #fff; padding: 30px; border-radius: 12px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); }}
        h1 {{ border-bottom: 2px solid #e2e8f0; padding-bottom: 15px; display: flex; align-items: center; gap: 10px; }}
        .stats-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 15px; margin-bottom: 30px; }}
        .stat-box {{ padding: 20px; border-radius: 8px; text-align: center; border: 1px solid #e2e8f0; background: #f8fafc; border-top-width: 4px; }}
        .stat-box h3 {{ margin: 10px 0 0; font-size: 2rem; color: var(--dark); line-height: 1; }}
        .stat-box p {{ margin: 5px 0 0; font-weight: 600; color: #64748b; font-size: 0.8rem; text-transform: uppercase; }}
        table {{ width: 100%; border-collapse: collapse; font-size: 0.9rem; }}
        th, td {{ padding: 10px; text-align: left; border-bottom: 1px solid #f1f5f9; }}
        th {{ background-color: #f8fafc; text-transform: uppercase; font-size: 0.8rem; }}
        .badge {{ padding: 4px 8px; border-radius: 4px; font-size: 0.75rem; font-weight: 700; display: inline-block; }}
    </style></head><body><div class="container">
    <h1><i class="fas fa-sync" style="color: var(--primary);"></i> Auditoría Empretienda</h1>
    <div class="stats-grid">
        <div class="stat-box" style="border-top-color: var(--success)"><i class="fas fa-check-circle fa-2x" style="color: var(--success)"></i><h3>{total_act}</h3><p>Actualizados</p></div>
        <div class="stat-box" style="border-top-color: var(--primary)"><i class="fas fa-plus-circle fa-2x" style="color: var(--primary)"></i><h3>{total_nuevos}</h3><p>Nuevos Inyectados</p></div>
        <div class="stat-box" style="border-top-color: var(--warning)"><i class="fas fa-battery-empty fa-2x" style="color: var(--warning)"></i><h3>{total_agotados}</h3><p>Agotados (Stock 0)</p></div>
        <div class="stat-box" style="border-top-color: var(--gray)"><i class="fas fa-eraser fa-2x" style="color: var(--gray)"></i><h3>{total_obsoletas}</h3><p>Var. Obsoleta (Basura)</p></div>
        <div class="stat-box" style="border-top-color: var(--darkgray)"><i class="fas fa-dumpster fa-2x" style="color: var(--darkgray)"></i><h3>{total_inexistentes}</h3><p>Prod. Inexistente (Basura)</p></div>
        <div class="stat-box" style="border-top-color: var(--danger)"><i class="fas fa-exclamation-triangle fa-2x" style="color: var(--danger)"></i><h3>{total_faltantes}</h3><p>Faltantes (No subidos)</p></div>
    </div><table><thead><tr><th>SKU</th><th>Estado</th><th>Producto</th><th>Variante</th><th>Stock</th><th>Precio</th></tr></thead><tbody>'''

    sort_order = {"Nuevo Agregado":0, "Faltante en Tienda":1, "Producto Inexistente (Oculto)":2, "Variante Obsoleta (Oculta)":3, "Agotado (Stock 0)":4, "Actualizado":5}
    logs.sort(key=lambda x: sort_order.get(x['accion'], 6))

    for log in logs:
        acc = log['accion']
        bg = "#d1fae5" if acc == "Actualizado" else ("#bfdbfe" if acc == "Nuevo Agregado" else ("#fef3c7" if "Agotado" in acc else ("#f1f5f9" if "Obsoleta" in acc else "#fee2e2")))
        html_content += f"<tr><td><b>{log['sku']}</b></td><td><span class='badge' style='background:{bg}; color:#000;'>{acc}</span></td><td>{log['nombre']}</td><td>{log['variante']}</td><td>{log['stock_old']} -> <b>{log['stock_new']}</b></td><td>${log['precio_old']} -> <b>${log['precio_new']}</b></td></tr>"

    html_content += "</tbody></table></div></body></html>"

    with open(output_html, "w", encoding="utf-8") as lf:
        lf.write(html_content)

if __name__ == '__main__':
    try:
        if len(sys.argv) > 5:
            sync_empretienda(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5]))
    except Exception as e:
        import traceback; traceback.print_exc()
        sys.exit(1)