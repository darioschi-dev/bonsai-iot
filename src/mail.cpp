#include "mail.h"
#include <ESP_Mail_Client.h>

static SMTPSession smtp;
static Session_Config mailSession;
static SMTP_Message message;

static void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
}

void setup_mail(const String &htmlMsg,
                const String &author_email,
                const String &author_password,
                const String &recipient_email)
{
  smtp.debug(1);
  smtp.callback(smtpCallback);

  mailSession.server.host_name = "smtp.gmail.com";
  mailSession.server.port = 465;

  mailSession.login.email = author_email;
  mailSession.login.password = author_password;

  message.sender.name = F("Igrometer");
  message.sender.email = author_email;
  message.subject = F("Soil moisture alert");
  message.addRecipient("User", recipient_email);

  message.html.content = htmlMsg.c_str();
  message.html.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&mailSession)) {
    Serial.println("[MAIL] SMTP connection failed");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("[MAIL] Send failed: %s\n", smtp.errorReason().c_str());
  }
}
