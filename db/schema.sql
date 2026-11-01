PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS Org (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS Department (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    org_id INTEGER NOT NULL REFERENCES Org(id),
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS Position (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS EmploymentKind (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS WorkSchedule (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    kind INTEGER NOT NULL,
    normative_minutes_per_day INTEGER NOT NULL,
    shift_cycle TEXT NOT NULL DEFAULT '',
    anchor_date INTEGER
);

CREATE TABLE IF NOT EXISTS AbsenceKind (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    paid INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS TerminationReason (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS CalcItem (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    formula TEXT NOT NULL,
    role INTEGER NOT NULL,
    stage INTEGER NOT NULL,
    priority INTEGER NOT NULL,
    taxable INTEGER NOT NULL,
    included_in_contrib_base INTEGER NOT NULL,
    reduces_income_tax_base INTEGER NOT NULL,
    rounding_policy INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS PayrollPreset (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS PayrollPresetItem (
    preset_id INTEGER NOT NULL REFERENCES PayrollPreset(id) ON DELETE CASCADE,
    calc_item_id INTEGER NOT NULL REFERENCES CalcItem(id),
    PRIMARY KEY (preset_id, calc_item_id)
);

CREATE TABLE IF NOT EXISTS Employee (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    personnel_number TEXT NOT NULL,
    full_name TEXT NOT NULL,
    org_id INTEGER NOT NULL REFERENCES Org(id),
    department_id INTEGER NOT NULL REFERENCES Department(id),
    position_id INTEGER NOT NULL REFERENCES Position(id),
    employment_kind_id INTEGER NOT NULL REFERENCES EmploymentKind(id),
    work_schedule_id INTEGER NOT NULL REFERENCES WorkSchedule(id),
    payroll_preset_id INTEGER REFERENCES PayrollPreset(id),
    rate_cents INTEGER NOT NULL,
    rate_unit INTEGER NOT NULL,
    hire_date INTEGER NOT NULL,
    termination_date INTEGER
);

CREATE TABLE IF NOT EXISTS EmploymentEvent (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_id INTEGER NOT NULL REFERENCES Employee(id) ON DELETE CASCADE,
    event_date INTEGER NOT NULL,
    kind INTEGER NOT NULL,
    personnel_number TEXT,
    full_name TEXT,
    org_id INTEGER,
    department_id INTEGER,
    position_id INTEGER,
    employment_kind_id INTEGER,
    work_schedule_id INTEGER,
    payroll_preset_id INTEGER,
    rate_cents INTEGER,
    rate_unit INTEGER,
    termination_reason_id INTEGER
);

CREATE TABLE IF NOT EXISTS TimeFact (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_id INTEGER NOT NULL REFERENCES Employee(id) ON DELETE CASCADE,
    fact_date INTEGER NOT NULL,
    worked_minutes INTEGER NOT NULL,
    absence_kind_id INTEGER REFERENCES AbsenceKind(id)
);

CREATE TABLE IF NOT EXISTS OutputFact (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_id INTEGER NOT NULL REFERENCES Employee(id),
    fact_date INTEGER NOT NULL,
    code TEXT NOT NULL,
    value_numerator INTEGER NOT NULL,
    value_denominator INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS PayrollPeriod (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    date_from INTEGER NOT NULL,
    date_to INTEGER NOT NULL,
    status INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS Calculation (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_id INTEGER NOT NULL REFERENCES Employee(id),
    payroll_period_id INTEGER NOT NULL REFERENCES PayrollPeriod(id),
    status INTEGER NOT NULL,
    incomplete_reasons TEXT NOT NULL,
    total_accrual_cents INTEGER NOT NULL,
    total_pre_tax_deduction_cents INTEGER NOT NULL,
    income_tax_cents INTEGER NOT NULL,
    total_employer_contribution_cents INTEGER NOT NULL,
    total_post_tax_deduction_cents INTEGER NOT NULL,
    net_amount_cents INTEGER NOT NULL,
    engine_version TEXT NOT NULL,
    input_snapshot TEXT
);

CREATE TABLE IF NOT EXISTS CalculationLine (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    calculation_id INTEGER NOT NULL REFERENCES Calculation(id) ON DELETE CASCADE,
    calc_item_id INTEGER NOT NULL,
    calc_item_code TEXT NOT NULL,
    role INTEGER NOT NULL,
    stage INTEGER NOT NULL,
    priority INTEGER NOT NULL,
    amount_cents INTEGER NOT NULL,
    formula_snapshot TEXT NOT NULL,
    rounding_policy INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS PaymentDoc (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_id INTEGER NOT NULL REFERENCES Employee(id),
    payroll_period_id INTEGER NOT NULL REFERENCES PayrollPeriod(id),
    payment_date INTEGER NOT NULL,
    channel INTEGER NOT NULL,
    amount_cents INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS PaymentLine (
    payment_doc_id INTEGER NOT NULL REFERENCES PaymentDoc(id) ON DELETE CASCADE,
    calculation_id INTEGER NOT NULL REFERENCES Calculation(id),
    amount_cents INTEGER NOT NULL,
    PRIMARY KEY (payment_doc_id, calculation_id)
);
