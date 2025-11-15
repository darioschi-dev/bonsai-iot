#pragma once
#include <Arduino.h>

void setup_mail(const String& htmlMsg,
                const String& author_email,
                const String& author_password,
                const String& recipient_email);
