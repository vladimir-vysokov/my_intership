#include "domain.hpp"

#include <iomanip>
#include <sstream>

namespace internstart {

static int intField(const Row& row, const std::string& key) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return 0;
    return std::stoi(it->second);
}

static std::string stringField(const Row& row, const std::string& key) {
    auto it = row.find(key);
    return it == row.end() ? "" : it->second;
}

Company Company::FromRow(const Row& row) {
    Company company;
    company.id = intField(row, "id");
    company.name = stringField(row, "name");
    company.slug = stringField(row, "slug");
    company.description = stringField(row, "description");
    company.accent_color = stringField(row, "accent_color").empty() ? "#0e7490" : stringField(row, "accent_color");
    company.is_active = stringField(row, "is_active") != "0";
    return company;
}

Internship Internship::FromRow(const Row& row) {
    Internship internship;
    internship.id = intField(row, "id");
    internship.company_id = intField(row, "company_id");
    internship.title = stringField(row, "title");
    internship.company_name = stringField(row, "company_name");
    internship.city = stringField(row, "city");
    internship.work_format = workFormatFromString(stringField(row, "work_format"));
    internship.direction = stringField(row, "direction");
    internship.status = internshipStatusFromString(stringField(row, "status"));
    internship.is_published = stringField(row, "is_published") == "1";
    internship.short_description = stringField(row, "short_description");
    internship.full_description = stringField(row, "full_description");
    internship.source_url = stringField(row, "source_url");
    return internship;
}

ApplicationAttempt ApplicationAttempt::FromRow(const Row& row) {
    ApplicationAttempt attempt;
    attempt.id = intField(row, "id");
    attempt.internship_id = intField(row, "internship_id");
    attempt.status = applicationStatusFromString(stringField(row, "status"));
    attempt.marker_enabled = stringField(row, "marker_enabled") != "0";
    attempt.stage_completed = stringField(row, "stage_completed") == "1";
    return attempt;
}

bool CompanyRatingSummary::hasRatings() const {
    return count > 0;
}

std::string CompanyRatingSummary::averageText() const {
    if (!hasRatings()) return "Нет оценок";
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << average;
    return out.str();
}

WorkFormat workFormatFromString(const std::string& value) {
    if (value == "office") return WorkFormat::Office;
    if (value == "remote") return WorkFormat::Remote;
    if (value == "hybrid") return WorkFormat::Hybrid;
    return WorkFormat::Unknown;
}

std::string toString(WorkFormat value) {
    switch (value) {
        case WorkFormat::Office: return "office";
        case WorkFormat::Remote: return "remote";
        case WorkFormat::Hybrid: return "hybrid";
        case WorkFormat::Unknown: return "";
    }
    return "";
}

std::string label(WorkFormat value) {
    switch (value) {
        case WorkFormat::Office: return "Офис";
        case WorkFormat::Remote: return "Удалённо";
        case WorkFormat::Hybrid: return "Гибрид";
        case WorkFormat::Unknown: return "Не указано";
    }
    return "Не указано";
}

InternshipStatus internshipStatusFromString(const std::string& value) {
    if (value == "open") return InternshipStatus::Open;
    if (value == "closing_soon") return InternshipStatus::ClosingSoon;
    if (value == "closed") return InternshipStatus::Closed;
    return InternshipStatus::Unknown;
}

std::string toString(InternshipStatus value) {
    switch (value) {
        case InternshipStatus::Open: return "open";
        case InternshipStatus::ClosingSoon: return "closing_soon";
        case InternshipStatus::Closed: return "closed";
        case InternshipStatus::Unknown: return "";
    }
    return "";
}

std::string label(InternshipStatus value) {
    switch (value) {
        case InternshipStatus::Open: return "Набор открыт";
        case InternshipStatus::ClosingSoon: return "Скоро закрывается";
        case InternshipStatus::Closed: return "Набор завершен";
        case InternshipStatus::Unknown: return "Не указан";
    }
    return "Не указан";
}

ApplicationStatus applicationStatusFromString(const std::string& value) {
    if (value == "want_to_apply") return ApplicationStatus::WantToApply;
    if (value == "applied") return ApplicationStatus::Applied;
    if (value == "test") return ApplicationStatus::Test;
    if (value == "interview") return ApplicationStatus::Interview;
    if (value == "offer") return ApplicationStatus::Offer;
    if (value == "rejected") return ApplicationStatus::Rejected;
    if (value == "archived") return ApplicationStatus::Archived;
    return ApplicationStatus::Unknown;
}

std::string toString(ApplicationStatus value) {
    switch (value) {
        case ApplicationStatus::WantToApply: return "want_to_apply";
        case ApplicationStatus::Applied: return "applied";
        case ApplicationStatus::Test: return "test";
        case ApplicationStatus::Interview: return "interview";
        case ApplicationStatus::Offer: return "offer";
        case ApplicationStatus::Rejected: return "rejected";
        case ApplicationStatus::Archived: return "archived";
        case ApplicationStatus::Unknown: return "want_to_apply";
    }
    return "want_to_apply";
}

std::string label(ApplicationStatus value) {
    switch (value) {
        case ApplicationStatus::WantToApply: return "Хочу податься";
        case ApplicationStatus::Applied: return "Подал";
        case ApplicationStatus::Test: return "Тест";
        case ApplicationStatus::Interview: return "Интервью";
        case ApplicationStatus::Offer: return "Оффер";
        case ApplicationStatus::Rejected: return "Отказ";
        case ApplicationStatus::Archived: return "Архив";
        case ApplicationStatus::Unknown: return "Не указан";
    }
    return "Не указан";
}

} // namespace internstart
