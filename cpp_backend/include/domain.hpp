#pragma once

#include <map>
#include <string>
#include <vector>

namespace internstart {

using Row = std::map<std::string, std::string>;
using Params = std::vector<std::string>;

enum class WorkFormat {
    Office,
    Remote,
    Hybrid,
    Unknown
};

enum class InternshipStatus {
    Open,
    ClosingSoon,
    Closed,
    Unknown
};

enum class ApplicationStatus {
    WantToApply,
    Applied,
    Test,
    Interview,
    Offer,
    Rejected,
    Archived,
    Unknown
};

struct Company {
    int id = 0;
    std::string name;
    std::string slug;
    std::string description;
    std::string accent_color = "#0e7490";
    bool is_active = true;

    static Company FromRow(const Row& row);
};

struct Internship {
    int id = 0;
    int company_id = 0;
    std::string title;
    std::string company_name;
    std::string city;
    WorkFormat work_format = WorkFormat::Unknown;
    std::string direction;
    InternshipStatus status = InternshipStatus::Unknown;
    bool is_published = false;
    std::string short_description;
    std::string full_description;
    std::string source_url;

    static Internship FromRow(const Row& row);
};

struct ApplicationAttempt {
    int id = 0;
    int internship_id = 0;
    ApplicationStatus status = ApplicationStatus::Unknown;
    bool marker_enabled = true;
    bool stage_completed = true;

    static ApplicationAttempt FromRow(const Row& row);
};

struct CompanyRatingSummary {
    double average = 0.0;
    int count = 0;
    int stars[5] = {0, 0, 0, 0, 0};

    bool hasRatings() const;
    std::string averageText() const;
};

WorkFormat workFormatFromString(const std::string& value);
std::string toString(WorkFormat value);
std::string label(WorkFormat value);

InternshipStatus internshipStatusFromString(const std::string& value);
std::string toString(InternshipStatus value);
std::string label(InternshipStatus value);

ApplicationStatus applicationStatusFromString(const std::string& value);
std::string toString(ApplicationStatus value);
std::string label(ApplicationStatus value);

} // namespace internstart
