#include "mail.h"
#include <WiFiClientSecure.h>
#include "mbedtls/base64.h"

// ------------------------------------------------------------
// Base64 helper
// ------------------------------------------------------------
String base64Encode(const String &data) {
    size_t out_len = 0;
    size_t in_len  = data.length();
    unsigned char out[256];

    mbedtls_base64_encode(out, sizeof(out), &out_len,
                          (const unsigned char *)data.c_str(), in_len);

    return String((char *)out);
}

// ------------------------------------------------------------
// SMTP SEND MAIL
// ------------------------------------------------------------
bool sendMail(const String& subject, const String& body)
{
    WiFiClientSecure client;
    client.setInsecure();

    const char* smtp_server = "smtp.gmail.com";
    const int smtp_port = 465;
    const char* username = "xxx@gmail.com";
    const char* password = "xxxx xxxx xxxx xxxx";

    Serial.println("[MAIL] Connessione al server SMTP...");

    if (!client.connect(smtp_server, smtp_port)) {
        Serial.println("[MAIL] Connessione SMTP fallita");
        return false;
    }

    auto send = [&](const String& s) {
        client.println(s);
        delay(30);
    };

    send("EHLO esp32");
    send("AUTH LOGIN");

    send(base64Encode(username));
    send(base64Encode(password));

    send("MAIL FROM:<" + String(username) + ">");
    send("RCPT TO:<" + String(username) + ">");
    send("DATA");

    send("Subject: " + subject);
    send("From: ESP32 <" + String(username) + ">");
    send("To: <" + String(username) + ">");
    send("");

    send(body);

    send(".");
    send("QUIT");

    Serial.println("[MAIL] Email inviata con successo!");
    return true;
}
