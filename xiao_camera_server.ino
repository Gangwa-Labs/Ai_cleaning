#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include "config.h"

// ==========================================
// XIAO ESP32S3 SENSE PIN MAPPING
// ==========================================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

WebServer server(80);

// Global buffer for the last captured frame
uint8_t* last_image_buf = NULL;
size_t last_image_len = 0;

// ==========================================
// FRONTEND HTML / CSS / JS
// ==========================================
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Room Judge AI</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600&display=swap');
    body { font-family: 'Poppins', sans-serif; background-color: #f4f7f6; color: #333; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .card { background: white; border-radius: 16px; box-shadow: 0 10px 20px rgba(0,0,0,0.1); padding: 24px; max-width: 500px; width: 100%; text-align: center; margin-bottom: 20px;}
    h1 { margin-top: 0; font-size: 24px; color: #2c3e50; }
    .btn-group { display: flex; gap: 10px; justify-content: center; margin-bottom: 20px; flex-wrap: wrap;}
    button { font-family: 'Poppins', sans-serif; font-weight: 600; padding: 12px 20px; border: none; border-radius: 8px; cursor: pointer; transition: all 0.2s; font-size: 16px; display: flex; align-items: center; justify-content: center; gap: 8px; flex: 1; min-width: 160px;}
    button:disabled { opacity: 0.5; cursor: not-allowed; transform: none !important; }
    .btn-capture { background: #3498db; color: white; }
    .btn-capture:hover:not(:disabled) { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(52,152,219,0.3);}
    .btn-judge { background: #2ecc71; color: white; }
    .btn-judge:hover:not(:disabled) { background: #27ae60; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(46,204,113,0.3);}
    #image-container { width: 100%; max-width: 450px; aspect-ratio: 4/3; background: #e0e6ed; border-radius: 12px; margin: 0 auto 20px; display: flex; align-items: center; justify-content: center; overflow: hidden; border: 2px dashed #bdc3c7;}
    img { width: 100%; height: 100%; object-fit: cover; display: none; }
    #img-placeholder { color: #7f8c8d; font-size: 14px; padding: 20px; }
    .progress-container { width: 100%; background: #e0e6ed; border-radius: 20px; height: 24px; overflow: hidden; margin-bottom: 16px; position: relative; }
    .progress-bar { height: 100%; width: 0%; background: linear-gradient(90deg, #e74c3c, #f1c40f, #2ecc71); transition: width 1s cubic-bezier(0.4, 0, 0.2, 1); }
    #results { display: none; text-align: left; }
    .comment-box { padding: 16px; border-radius: 8px; background: #f8f9fa; border-left: 5px solid #bdc3c7; font-style: italic; margin-bottom: 16px; transition: border-color 0.3s; }
    h3 { margin-bottom: 8px; color: #2c3e50; font-size: 18px; }
    ul { list-style: none; padding: 0; margin: 0; }
    li { background: #f8f9fa; margin-bottom: 8px; padding: 12px; border-radius: 8px; display: flex; align-items: flex-start; gap: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.02); border: 1px solid #eee;}
    input[type="checkbox"] { width: 20px; height: 20px; accent-color: #2ecc71; cursor: pointer; flex-shrink: 0; margin-top: 2px; }
    .strikethrough { text-decoration: line-through; color: #95a5a6; }
    .loader { border: 3px solid rgba(255,255,255,0.3); border-top: 3px solid white; border-radius: 50%; width: 16px; height: 16px; animation: spin 1s linear infinite; display: none; }
    @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <div class="card">
    <h1>Room Judge AI ✨</h1>
    <div class="btn-group">
      <button class="btn-capture" id="btn-capture" onclick="captureImage()">
        <div class="loader" id="load-capture"></div> 📸 1. Capture Image
      </button>
      <button class="btn-judge" id="btn-judge" onclick="analyzeImage()" disabled>
        <div class="loader" id="load-judge"></div> ✨ 2. Judge Room
      </button>
    </div>
    <div id="image-container">
      <span id="img-placeholder">Camera is idle. Click Capture.</span>
      <img id="photo" src="" alt="Captured Frame">
    </div>
    
    <div id="results">
      <h3>Cleanliness Score: <span id="score-text">0</span>%</h3>
      <div class="progress-container">
        <div class="progress-bar" id="progress-bar"></div>
      </div>
      <div class="comment-box" id="comment-box"></div>
      <h3>To-Do List</h3>
      <ul id="todo-list"></ul>
    </div>
  </div>

  <script>
    async function captureImage() {
      const btnCap = document.getElementById('btn-capture');
      const btnJudge = document.getElementById('btn-judge');
      const loaderCap = document.getElementById('load-capture');
      const img = document.getElementById('photo');
      const placeholder = document.getElementById('img-placeholder');
      const results = document.getElementById('results');
      
      btnCap.disabled = true;
      btnJudge.disabled = true;
      loaderCap.style.display = 'block';
      results.style.display = 'none';
      
      try {
        const res = await fetch('/capture');
        if(res.ok) {
          img.src = '/last_image?t=' + Date.now();
          img.style.display = 'block';
          placeholder.style.display = 'none';
          document.getElementById('image-container').style.border = 'none';
          btnJudge.disabled = false;
        } else {
          alert('Capture failed! Check serial monitor.');
        }
      } catch (e) {
        alert('Network error during capture.');
      }
      btnCap.disabled = false;
      loaderCap.style.display = 'none';
    }

    async function analyzeImage() {
      const btnJudge = document.getElementById('btn-judge');
      const btnCap = document.getElementById('btn-capture');
      const loaderJudge = document.getElementById('load-judge');
      const results = document.getElementById('results');
      const progressBar = document.getElementById('progress-bar');
      const scoreText = document.getElementById('score-text');
      const commentBox = document.getElementById('comment-box');
      const todoList = document.getElementById('todo-list');
      
      btnJudge.disabled = true;
      btnCap.disabled = true;
      loaderJudge.style.display = 'block';
      
      results.style.display = 'block';
      progressBar.style.width = '0%';
      scoreText.innerText = '...';
      commentBox.innerText = 'Analyzing your mess...';
      todoList.innerHTML = '';
      commentBox.style.borderColor = '#bdc3c7';

      try {
        const res = await fetch('/analyze');
        const data = await res.json();
        
        if (data.error) {
          commentBox.innerText = 'Error: ' + data.error;
          commentBox.style.borderColor = '#e74c3c';
        } else {
          let score = data.score || 0;
          scoreText.innerText = score;
          setTimeout(() => { progressBar.style.width = score + '%'; }, 100);
          
          commentBox.innerText = `"${data.comment}"`;
          if(score < 40) commentBox.style.borderColor = '#e74c3c';
          else if(score < 70) commentBox.style.borderColor = '#f1c40f';
          else commentBox.style.borderColor = '#2ecc71';

          const tasks = data.tasks || [];
          tasks.forEach(task => {
            const li = document.createElement('li');
            const cb = document.createElement('input');
            cb.type = 'checkbox';
            const span = document.createElement('span');
            span.innerText = task;
            cb.onchange = (e) => {
              if(e.target.checked) span.classList.add('strikethrough');
              else span.classList.remove('strikethrough');
            };
            li.appendChild(cb);
            li.appendChild(span);
            todoList.appendChild(li);
          });
        }
      } catch (e) {
        commentBox.innerText = 'Failed to parse response. Check API key & serial monitor.';
        commentBox.style.borderColor = '#e74c3c';
      }
      
      btnJudge.disabled = false;
      btnCap.disabled = false;
      loaderJudge.style.display = 'none';
    }
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// UTILITY FUNCTIONS
// ==========================================
String base64Encode(uint8_t* buf, size_t len) {
  size_t out_len;
  mbedtls_base64_encode(NULL, 0, &out_len, buf, len);
  unsigned char* out_buf = (unsigned char*)malloc(out_len);
  if(out_buf == NULL) return "";
  mbedtls_base64_encode(out_buf, out_len, &out_len, buf, len);
  String res = (char*)out_buf;
  free(out_buf);
  return res;
}

// ==========================================
// ENDPOINT HANDLERS
// ==========================================
void handleRoot() {
  server.send(200, "text/html", html_page);
}

void handleCapture() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "application/json", "{\"error\":\"Camera capture failed\"}");
    return;
  }
  
  // Free old buffer if it exists
  if (last_image_buf != NULL) {
    free(last_image_buf);
    last_image_buf = NULL;
  }
  
  last_image_len = fb->len;
  last_image_buf = (uint8_t*)ps_malloc(last_image_len);
  
  if (last_image_buf != NULL) {
    memcpy(last_image_buf, fb->buf, last_image_len);
  }
  
  esp_camera_fb_return(fb); // Release hardware buffer immediately
  
  if (last_image_buf == NULL) {
    server.send(500, "application/json", "{\"error\":\"PSRAM allocation failed\"}");
    return;
  }
  
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleLastImage() {
  if (last_image_buf != NULL && last_image_len > 0) {
    server.setContentLength(last_image_len);
    server.send(200, "image/jpeg", "");
    server.client().write(last_image_buf, last_image_len);
  } else {
    server.send(404, "text/plain", "No image found");
  }
}

void handleAnalyze() {
  if (last_image_buf == NULL || last_image_len == 0) {
    server.send(400, "application/json", "{\"error\":\"No image captured yet\"}");
    return;
  }

  Serial.println("Encoding image to Base64...");
  String base64Image = base64Encode(last_image_buf, last_image_len);
  if (base64Image == "") {
    server.send(500, "application/json", "{\"error\":\"Base64 encoding failed\"}");
    return;
  }

  Serial.println("Sending to OpenAI API...");
  WiFiClientSecure client;
  client.setInsecure(); // Bypass SSL verification for simplicity on ESP32
  HTTPClient https;

  if (https.begin(client, "https://api.openai.com/v1/chat/completions")) {
    https.addHeader("Content-Type", "application/json");
    String authHeader = "Bearer ";
    authHeader += openai_api_key;
    https.addHeader("Authorization", authHeader);
    https.setTimeout(35000); // 35 seconds timeout - OpenAI Vision can be slow

    // Construct JSON payload manually to prevent memory fragmentation
    String payload;
    payload.reserve(base64Image.length() + 1024);
    payload += "{\n";
    payload += "\"model\": \"gpt-5.4-mini\",\n";
    payload += "\"response_format\": {\"type\": \"json_object\"},\n";
    payload += "\"messages\": [\n";
    payload += "  {\n";
    payload += "    \"role\": \"system\",\n";
    payload += "    \"content\": \"You are a room cleanliness judge. Output JSON with exactly: 'score' (integer 0-100), 'comment' (string: insult if < 50, compliment if >= 50, scale intensity with score), 'tasks' (array of strings, cleaning steps).\"\n";
    payload += "  },\n";
    payload += "  {\n";
    payload += "    \"role\": \"user\",\n";
    payload += "    \"content\": [\n";
    payload += "      {\"type\": \"text\", \"text\": \"Analyze this room.\"},\n";
    payload += "      {\"type\": \"image_url\", \"image_url\": {\"url\": \"data:image/jpeg;base64," + base64Image + "\"}}\n";
    payload += "    ]\n";
    payload += "  }\n";
    payload += "]\n";
    payload += "}";

    int httpCode = https.POST(payload);
    
    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      Serial.println("Received API Response!");
      
      // Parse the response
      #if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
      #else
        DynamicJsonDocument doc(8192);
      #endif
      
      DeserializationError error = deserializeJson(doc, response);
      if (!error) {
        String aiJsonContent = doc["choices"][0]["message"]["content"].as<String>();
        server.send(200, "application/json", aiJsonContent);
      } else {
        server.send(500, "application/json", "{\"error\":\"Failed to parse JSON response\"}");
        Serial.println("JSON Parse Error");
      }
    } else {
      String errResponse = https.getString();
      server.send(500, "application/json", "{\"error\":\"OpenAI API HTTP Error " + String(httpCode) + "\"}");
      Serial.printf("API Error %d: %s\n", httpCode, errResponse.c_str());
    }
    https.end();
  } else {
    server.send(500, "application/json", "{\"error\":\"Unable to connect to OpenAI endpoint\"}");
  }
}

// ==========================================
// SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Give Serial monitor time to open
  
  WiFi.mode(WIFI_STA); // Explicitly set station mode
  WiFi.setSleep(false); // Prevents web server dropping connections
  
  Serial.println();
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  // Timeout after 15 seconds to prevent hanging forever
  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 30) {
    delay(500);
    Serial.print(".");
    wifi_retries++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[ERROR] Failed to connect to WiFi!");
    Serial.println("Please check that your SSID and Password are correct.");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }
  
  Serial.println("\nWiFi connected.");
  Serial.print("Access UI at: http://");
  Serial.println(WiFi.localIP());

  // Configure Camera for XIAO ESP32S3 Sense
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  // Resolution & Memory limits (Keep relatively low to prevent Base64/API explosion)
  config.frame_size = FRAMESIZE_VGA; 
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Only grab when requested
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
  Serial.println("Camera configured successfully.");

  // Map HTTP Endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/last_image", HTTP_GET, handleLastImage);
  server.on("/analyze", HTTP_GET, handleAnalyze);

  server.begin();
  Serial.println("Web Server active.");
}

void loop() {
  server.handleClient();
  delay(2);
}
