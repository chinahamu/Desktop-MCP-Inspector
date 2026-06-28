#include "HTMLReportWriter.hpp"
#include "MarkdownReportWriter.hpp"
#include "ReportModel.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QDateTime>
#include <QString>

TEST_CASE("MarkdownReportWriter renders security and timeline snapshot")
{
  security::SecurityScanResult security;
  security.score = security::RiskScore{
      75,
      security::RiskSeverity::Medium,
      QStringLiteral("1 security finding(s), score 75/100, highest severity: medium"),
  };
  security.rule_count = 6;
  security.passed = false;
  security.findings.push_back(security::RiskFinding{
      QStringLiteral("demo.review"),
      security::RiskSeverity::Medium,
      security::RiskCategory::ToolMetadata,
      QStringLiteral("Review-needed tool"),
      QStringLiteral("Tool requires an operator review before use."),
      QStringLiteral("Review server trust and narrow the available actions."),
      QStringLiteral("tools/review.action"),
  });

  std::vector<timeline::TimelineEvent> events;
  timeline::TimelineEvent request;
  request.direction = timeline::TimelineDirection::Outbound;
  request.kind = timeline::TimelineEventKind::Request;
  request.status = timeline::TimelineStatus::Success;
  request.method = QStringLiteral("tools/list");
  request.summary = QStringLiteral("List tools");
  request.duration_ms = 40;
  events.push_back(request);

  timeline::TimelineEvent response;
  response.direction = timeline::TimelineDirection::Inbound;
  response.kind = timeline::TimelineEventKind::Response;
  response.status = timeline::TimelineStatus::Error;
  response.method = QStringLiteral("tools/list");
  response.summary = QStringLiteral("List tools failed");
  response.duration_ms = 60;
  events.push_back(response);

  timeline::TimelineEvent lifecycle;
  lifecycle.direction = timeline::TimelineDirection::Internal;
  lifecycle.kind = timeline::TimelineEventKind::Lifecycle;
  lifecycle.status = timeline::TimelineStatus::Success;
  lifecycle.method = QStringLiteral("app/startup");
  lifecycle.summary = QStringLiteral("Started");
  events.push_back(lifecycle);

  const auto generated_at = QDateTime::fromString(QStringLiteral("2026-01-02T03:04:05.000Z"), Qt::ISODateWithMs);
  const auto model = report::make_report_model(
      security,
      events,
      QStringLiteral("Snapshot Report"),
      QStringLiteral("demo-profile"),
      generated_at);

  const auto markdown = report::MarkdownReportWriter{}.write(model);
  const auto expected = QStringLiteral(R"(# Snapshot Report

- Generated: 2026-01-02T03:04:05.000Z
- Profile: demo-profile

## Security score

- Score: 75/100 (medium)
- Summary: 1 security finding(s), score 75/100, highest severity: medium
- Rules evaluated: 6
- Findings: 1
- Status: review required

## Security findings

| Severity | Category | Title | Subject | Recommendation |
| --- | --- | --- | --- | --- |
| medium | tool.metadata | Review-needed tool | tools/review.action | Review server trust and narrow the available actions. |

## Timeline summary

- Total events: 3
- Directions: inbound 1, outbound 1, internal 1
- Kinds: request 1, response 1, notification 0, stderr 0, transport error 0, lifecycle 1
- Statuses: errors 1, logs 0
- Average measured duration: 50.0 ms

### Timeline method summary

| Method | Events | Errors | Average duration |
| --- | ---: | ---: | --- |
| tools/list | 2 | 1 | 50.0 ms |
| app/startup | 1 | 0 | n/a |

)");

  CHECK(markdown == expected);
}

TEST_CASE("HTMLReportWriter renders escaped report document")
{
  security::SecurityScanResult security;
  security.score = security::RiskScore{100, security::RiskSeverity::Info, QStringLiteral("No security findings")};
  security.rule_count = 6;
  security.passed = true;

  const auto model = report::make_report_model(
      security,
      {},
      QStringLiteral("Escaped <Report>"),
      QStringLiteral("profile & demo"),
      QDateTime::fromString(QStringLiteral("2026-01-02T03:04:05.000Z"), Qt::ISODateWithMs));

  const auto html = report::HTMLReportWriter{}.write(model);
  CHECK(html.contains(QStringLiteral("<!doctype html>")));
  CHECK(html.contains(QStringLiteral("Escaped &lt;Report&gt;")));
  CHECK(html.contains(QStringLiteral("profile &amp; demo")));
  CHECK(html.contains(QStringLiteral("No security findings were detected.")));
}
