import pdfplumber
import json
import os
import sys
import re 
import time
from PIL import Image

try:
    from google import genai
    from openai import OpenAI
except ImportError:
    print("⚠️ Faltan librerías. Asegúrate de tener 'google-genai', 'openai' y 'python3-pil' instaladas.")
    sys.exit(1)


GEMINI_API_KEY = "Gemini_Api_Key_Aqui"
GROQ_API_KEY = "Groq_Api_Key" 

def llamar_gemini_2_5(prompt):
    print("  -> Intentando con Gemini 2.0 Flash...")
    client = genai.Client(api_key=GEMINI_API_KEY)
    response = client.models.generate_content(
        model='gemini-2.0-flash', 
        contents=prompt,
    )
    return response.text

def llamar_llama_meta(prompt):
    print("  -> Intentando con Llama 3 vía Groq (con recorte)...")
    prompt_seguro = prompt[:25000] + "\n...[TEXTO RECORTADO POR LÍMITE DE TOKENS]..." if len(prompt) > 25000 else prompt
    client = OpenAI(api_key=GROQ_API_KEY, base_url="https://api.groq.com/openai/v1")
    response = client.chat.completions.create(
        model="llama-3.3-70b-versatile",
        messages=[{"role": "user", "content": prompt_seguro}],
        temperature=0.1,
        max_tokens=4000,
        response_format={"type": "json_object"} 
    )
    return response.choices[0].message.content





def es_pdf(filepath):
    try:
        with open(filepath, 'rb') as f:
            return f.read(4) == b'%PDF'
    except:
        return False

def extraer_texto_pdf(pdf_path):
    print(f"📄 Extrayendo texto de {pdf_path}...")
    texto_completo = ""
    with pdfplumber.open(pdf_path) as pdf:
        for page in pdf.pages:
            texto_completo += page.extract_text() + "\n"
            
    texto_comprimido = re.sub(r'\s+', ' ', texto_completo).strip()
    return texto_comprimido

def procesar_imagen_multimodal(image_path, prompt):
    print(f"🖼️ Detectada imagen. Analizando {image_path} con Gemini 2.0 Flash...")
    intentos = 0
    while intentos < 2:
        try:
            img = Image.open(image_path)
            client = genai.Client(api_key=GEMINI_API_KEY)
            
            response = client.models.generate_content(
                model='gemini-2.0-flash',
                contents=[img, prompt]
            )
            
            resultado_crudo = response.text
            inicio = resultado_crudo.find('[')
            fin = resultado_crudo.rfind(']')
            if inicio != -1 and fin != -1 and fin >= inicio:
                return json.loads(resultado_crudo[inicio:fin+1])
            else:
                return json.loads(resultado_crudo.replace("```json", "").replace("```", "").strip())
                
        except Exception as e:
            error_str = str(e)
            if "429" in error_str or "Resource Exhausted" in error_str:
                match = re.search(r'retry in (\d+\.?\d*)s', error_str)
                wait_time = float(match.group(1)) + 2 if match else 60
                print(f"⚠️ Límite de cuota alcanzado. Esperando {wait_time:.0f} segundos antes de reintentar...")
                time.sleep(wait_time)
                intentos += 1
                continue
                
            msj = f"Error procesando imagen: {error_str}"
            print(f"⚠️ {msj}")
            with open("error_ia_msg.json", "w", encoding="utf-8") as f:
                json.dump({"error_detallado": msj}, f, ensure_ascii=False)
            sys.exit(1)

def procesar_con_ia(texto_pdf, prompt_base):
    print("🧠 Enviando a la IA para organizar los datos...")
    
    prompt = prompt_base + "\nTEXTO DEL PDF:\n" + texto_pdf
    motores_ia = [llamar_gemini_2_5, llamar_llama_meta]
    errores_recolectados = [] 

    for motor in motores_ia:
        intentos = 0
        while intentos < 2:
            try:
                prompt_enviado = prompt if motor.__name__ == "llamar_gemini_2_5" else prompt + "\nDevuelve un objeto JSON con una clave 'datos' que contenga el array."
                
                resultado_crudo = motor(prompt_enviado)
                
                inicio = resultado_crudo.find('[')
                fin = resultado_crudo.rfind(']')
                
                if inicio != -1 and fin != -1 and fin >= inicio:
                    resultado_limpio = resultado_crudo[inicio:fin+1]
                else:
                    resultado_limpio = resultado_crudo.replace("```json", "").replace("```", "").strip()
                    
                datos_json = json.loads(resultado_limpio)
                
                if isinstance(datos_json, dict) and "datos" in datos_json:
                    datos_json = datos_json["datos"]
                
                print(f"✅ ¡Éxito estructurando JSON usando {motor.__name__}!")
                return datos_json
                
            except json.JSONDecodeError as e:
                msj = f"{motor.__name__}: JSON incompleto o mal formado."
                print(f"⚠️ {msj}")
                errores_recolectados.append(msj)
                break 
                
            except Exception as e:
                error_str = str(e)
                
                if "429" in error_str or "quota" in error_str.lower() or "resource_exhausted" in error_str.lower():
                    match = re.search(r'retry in (\d+\.?\d*)s', error_str)
                    wait_time = float(match.group(1)) + 2 if match else 60 
                    print(f"⚠️ {motor.__name__} límite alcanzado. Pausando {wait_time:.0f} segundos y reintentando...")
                    time.sleep(wait_time)
                    intentos += 1
                    continue 
                    
                msj = f"{motor.__name__}: {error_str}"
                print(f"⚠️ {msj}")
                errores_recolectados.append(msj)
                break 
            
    print("❌ Error fatal: Todas las IAs fallaron.")
    
    mensaje_final = "\n".join(errores_recolectados)
    with open("error_ia_msg.json", "w", encoding="utf-8") as f:
        json.dump({"error_detallado": mensaje_final}, f, ensure_ascii=False)
        
    sys.exit(1)

if __name__ == "__main__":
    archivo = sys.argv[1] if len(sys.argv) > 1 else "temp_presupuesto.pdf"
    
    if not os.path.exists(archivo):
        print(f"❌ ERROR: No se encontró el archivo '{archivo}'.")
        sys.exit(1)
        
    prompt_base = """
    Sos un asistente experto en extracción y estructuración de datos a partir de remitos o facturas. Tu trabajo es analizar el texto provisto y devolverme ÚNICAMENTE un array JSON válido, sin bloques de código markdown ni explicaciones adicionales.
    
    REGLAS ESTRICTAS DE PROCESAMIENTO:
    
    1. LIMPIEZA DE SKU: El campo 'm_sku' debe contener ÚNICAMENTE números. Elimina los corchetes y cualquier prefijo de letras (por ejemplo, si el código es '[BGX1622]' o '[BEL4192]', el SKU debe ser estrictamente '1622' o '4192').
    2. FORMATO DE TEXTO (Title Case): La marca ('m_marca.m_name') y los colores ('m_color') deben tener solo la primera letra en mayúscula y el resto en minúscula (Ejemplo: 'Brigitte', 'Blanco', 'Oliva'). No uses TODO MAYÚSCULAS.
    3. AGRUPACIÓN (CORTE DE PÁGINA): Presta mucha atención. Agrupa TODAS las variantes (talles y colores) que pertenezcan al mismo producto bajo el mismo objeto JSON. Esto aplica incluso si las variantes aparecen separadas por saltos de página, encabezados, o la palabra "Subtotal" en el texto.
    4. MATEMÁTICA Y REDONDEO (m_precioBase): Toma el precio unitario base del producto, súmale un 27% (multiplicando por 1.27). Luego, REDONDEA HACIA ARRIBA para que quede un número amigable en pesos argentinos (múltiplos de 100). Prioriza terminaciones en 000 o 500. Por ejemplo, si el cálculo da 22800 o 22900, redondea directo a 23000. Si da 10300, puedes dejarlo ahí, pero evita números fraccionados o decimales raros.
    5. CANTIDADES: 'm_stock' es la cantidad indicada para esa variante exacta. 'm_precioEspecifico' siempre debe ser 0.0.
    
    ESTRUCTURA JSON REQUERIDA (Ejemplo):
    [
      {
        "m_sku": "1622",
        "m_nombre": "CONJUNTO TAZA SOFT DE PUNTILLA CON ELASTICO PERSONALIZADO",
        "m_categoria": { "m_name": "Lenceria" },
        "m_marca": { "m_name": "Brigitte" },
        "m_precioBase": 23400.0,
        "m_variantes": [
          { "m_codigoBarras": [], "m_talle": "100", "m_color": "Blanco", "m_stock": 1, "m_precioEspecifico": 0.0 },
          { "m_codigoBarras": [], "m_talle": "95", "m_color": "Oliva", "m_stock": 1, "m_precioEspecifico": 0.0 }
        ]
      }
    ]
    
    TEXTO DEL PDF:
    """
    
    if es_pdf(archivo):
        texto_crudo = extraer_texto_pdf(archivo)
        datos_estructurados = procesar_con_ia(texto_crudo, prompt_base)
    else:
        datos_estructurados = procesar_imagen_multimodal(archivo, prompt_base)
        
    archivo_salida = "temp_ia_output.json" 
    with open(archivo_salida, "w", encoding="utf-8") as f:
        json.dump(datos_estructurados, f, indent=4, ensure_ascii=False)
        
    print(f"✅ Proceso finalizado.")