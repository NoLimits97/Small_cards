INSERT OR IGNORE INTO Org(id, name)
VALUES (1, 'Основная организация');

INSERT OR IGNORE INTO Department(id, org_id, name)
VALUES (1, 1, 'Основное подразделение');

INSERT OR IGNORE INTO Position(id, name)
VALUES (1, 'Сотрудник');

INSERT OR IGNORE INTO EmploymentKind(id, code, name)
VALUES (1, 'main', 'Основное место работы');

INSERT OR IGNORE INTO WorkSchedule(
    id, name, kind, normative_minutes_per_day, shift_cycle, anchor_date)
VALUES (1, 'FiveDay', 1, 480, '', NULL);

INSERT OR IGNORE INTO AbsenceKind(id, code, name, paid)
VALUES (1, 'sick', 'Больничный', 1);

INSERT OR IGNORE INTO AbsenceKind(id, code, name, paid)
VALUES (2, 'unpaid', 'Отпуск без содержания', 0);

INSERT OR IGNORE INTO CalcItem(
    id, code, name, formula, role, stage, priority, taxable,
    included_in_contrib_base, reduces_income_tax_base, rounding_policy)
VALUES (
    1, 'oklad', 'Оклад', 'base_amount * worked_days / norm_days',
    1, 1, 0, 1, 1, 0, 1);

INSERT OR IGNORE INTO PayrollPreset(id, code, name)
VALUES (1, 'salary_proportional', 'Оклад пропорционально');

INSERT OR IGNORE INTO PayrollPresetItem(preset_id, calc_item_id)
VALUES (1, 1);
