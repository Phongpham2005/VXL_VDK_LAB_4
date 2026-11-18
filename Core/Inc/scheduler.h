/*
 * scheduler.h
 *
 *  Created on: Nov 12, 2026
 *      Author: Pham Dinh Phong (MSSV: 2312628)
 *  Description: Co-operative Time-Triggered Scheduler with O(1) Time Complexity
 *               implemented using Delta-Time Doubly Linked List and Static Task Pool.
 */

#ifndef INC_SCHEDULER_H_
#define INC_SCHEDULER_H_

#include <stdint.h>
#include <stddef.h>

#define SCH_MAX_TASKS   10
#define NO_TASK_ID      0

#define RETURN_NORMAL   0
#define RETURN_ERROR    1

#define ERROR_SCH_TOO_MANY_TASKS        1
#define ERROR_SCH_CANNOT_DELETE_TASK    2

// Cấu trúc node cho Danh sách liên kết đôi Delta-Time Queue quản lý Task
typedef struct sTask {
    void (*pTask)(void);    // Con trỏ hàm thực thi của tác vụ
    uint32_t Delay;         // Thời gian trễ tương đối (Delta delay tính bằng ticks)
    uint32_t Period;        // Chu kỳ lặp lại (tính bằng ticks, 0 nếu là One-shot)
    uint8_t  RunMe;         // Cờ / bộ đếm báo hiệu tác vụ sẵn sàng được thực thi
    uint32_t TaskID;        // ID định danh duy nhất của tác vụ (1..SCH_MAX_TASKS)
    struct sTask* pPrev;    // Con trỏ liên kết task đứng trước trong Delta Queue
    struct sTask* pNext;    // Con trỏ liên kết task tiếp theo trong Delta Queue
} sTask;

/* Khai báo các API tiêu chuẩn của bộ lập lịch */
void     SCH_Init(void);
void     SCH_Update(void);          // Đạt độ phức tạp O(1) tuyệt đối trong ngắt ISR Timer
void     SCH_Dispatch_Tasks(void);  // Điều phối thực thi các task sẵn sàng trong Super Loop
uint32_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY, uint32_t PERIOD);
uint8_t  SCH_Delete_Task(uint32_t taskID);

#endif /* INC_SCHEDULER_H_ */
