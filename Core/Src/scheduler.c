/*
 * scheduler.c
 *
 *  Created on: Nov 12, 2026
 *      Author: Pham Dinh Phong (MSSV: 2312628)
 *  Description: Co-operative Time-Triggered Scheduler with O(1) Time Complexity
 *               using Delta-Time Doubly Linked List and Static Task Pool.
 */

#include "scheduler.h"
#include "main.h"

// Bể chứa task tĩnh (Static Task Pool), hoàn toàn không dùng malloc/free trong hệ nhúng
static sTask task_pool[SCH_MAX_TASKS];

// Con trỏ quản lý danh sách các slot còn trống (Free List) để cấp phát và thu hồi O(1)
static sTask* free_head = NULL;

// Con trỏ đầu danh sách liên kết đôi sắp xếp theo thời gian tương đối (Delta-Time Queue Head)
static sTask* SCH_head_G = NULL;

// Biến lưu mã lỗi hệ thống
static uint8_t Error_code_G = 0;

/* Hàm nội bộ: Chèn node vào Delta-Time Queue theo thứ tự thời gian tăng dần */
static void SCH_Insert_To_Queue(sTask* newTask) {
    if (SCH_head_G == NULL) {
        // Hàng đợi rỗng: newTask trở thành head duy nhất
        SCH_head_G = newTask;
        newTask->pPrev = NULL;
        newTask->pNext = NULL;
        return;
    }

    sTask* current = SCH_head_G;
    sTask* prev = NULL;

    // Duyệt tìm vị trí chèn dựa trên độ trễ delta tích lũy
    while (current != NULL && newTask->Delay >= current->Delay) {
        newTask->Delay -= current->Delay;
        prev = current;
        current = current->pNext;
    }

    if (prev == NULL) {
        // Chèn vào vị trí đầu tiên của hàng đợi (Head)
        newTask->pNext = SCH_head_G;
        newTask->pPrev = NULL;
        SCH_head_G->pPrev = newTask;
        SCH_head_G = newTask;
        // Trừ thời gian delay của node đứng sau đi khoảng delay của newTask
        if (newTask->pNext != NULL) {
            newTask->pNext->Delay -= newTask->Delay;
        }
    } else {
        // Chèn vào giữa hoặc cuối hàng đợi
        newTask->pNext = current;
        newTask->pPrev = prev;
        prev->pNext = newTask;
        if (current != NULL) {
            current->pPrev = newTask;
            current->Delay -= newTask->Delay;
        }
    }
}

/* Khởi tạo bộ lập lịch và chuẩn bị Free List */
void SCH_Init(void) {
    unsigned char i;
    for (i = 0; i < SCH_MAX_TASKS; i++) {
        task_pool[i].pTask = NULL;
        task_pool[i].Delay = 0;
        task_pool[i].Period = 0;
        task_pool[i].RunMe = 0;
        task_pool[i].TaskID = i + 1;
        task_pool[i].pPrev = NULL;
        task_pool[i].pNext = (i < SCH_MAX_TASKS - 1) ? &task_pool[i + 1] : NULL;
    }
    free_head = &task_pool[0];
    SCH_head_G = NULL;
    Error_code_G = 0;
}

/*
 * --- ĐỘ PHỨC TẠP O(1) TUYỆT ĐỐI CHO HÀM SCH_UPDATE ---
 * Được gọi định kỳ bên trong ngắt phần cứng Timer (ví dụ 10ms/lần).
 * Do hàng đợi đã được tổ chức theo Delta-Time Queue, chỉ phần tử đứng đầu
 * danh sách (SCH_head_G) mới cần đếm lùi thời gian.
 * Tuyệt đối KHÔNG có vòng lặp, KHÔNG duyệt mảng -> Thời gian thực thi hằng số O(1).
 */
void SCH_Update(void) {
    if (SCH_head_G != NULL) {
        if (SCH_head_G->Delay > 0) {
            SCH_head_G->Delay--;
        }
        if (SCH_head_G->Delay == 0) {
            SCH_head_G->RunMe++;
        }
    }
}

/*
 * --- HÀM ĐIỀU PHỐI TÁC VỤ (DISPATCHER) ---
 * Được gọi liên tục trong vòng lặp vô tận while(1) của chương trình chính.
 * Lấy tác vụ đã đến hạn ở đầu hàng đợi Delta Queue để thực thi theo đúng thứ tự FIFO.
 * Nếu là task định kỳ, chèn lại vào hàng đợi trong thread context (không chặn ISR).
 */
void SCH_Dispatch_Tasks(void) {
    while (SCH_head_G != NULL && (SCH_head_G->RunMe > 0 || SCH_head_G->Delay == 0)) {
        __disable_irq();
        sTask* pTaskToRun = SCH_head_G;

        // Tách task ở đầu hàng đợi ra khỏi Delta Queue
        SCH_head_G = pTaskToRun->pNext;
        if (SCH_head_G != NULL) {
            SCH_head_G->pPrev = NULL;
        }
        pTaskToRun->pNext = NULL;
        pTaskToRun->pPrev = NULL;

        if (pTaskToRun->RunMe > 0) {
            pTaskToRun->RunMe--;
        }
        __enable_irq();

        // Thực thi tác vụ qua con trỏ hàm (ngắt đã được mở để không làm trễ tick ngắt)
        if (pTaskToRun->pTask != NULL) {
            (*pTaskToRun->pTask)();
        }

        __disable_irq();
        if (pTaskToRun->Period > 0) {
            // Tác vụ định kỳ: Nạp lại chu kỳ (Period) và tái chèn vào Delta Queue
            pTaskToRun->Delay = pTaskToRun->Period;
            pTaskToRun->RunMe = 0;
            SCH_Insert_To_Queue(pTaskToRun);
        } else {
            // Tác vụ một lần (One-shot): Thu hồi slot về Free List trong O(1)
            pTaskToRun->pTask = NULL;
            pTaskToRun->Delay = 0;
            pTaskToRun->Period = 0;
            pTaskToRun->RunMe = 0;
            pTaskToRun->pPrev = NULL;
            pTaskToRun->pNext = free_head;
            free_head = pTaskToRun;
        }
        __enable_irq();
    }
}

/*
 * --- THÊM TÁC VỤ VÀO BỘ LẬP LỊCH ---
 * Cấp phát slot từ Free List trong O(1) và chèn vào Delta-Time Queue.
 * DELAY và PERIOD tính theo mili-giây (được quy đổi sang số ticks với 1 tick = 10ms).
 */
uint32_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY, uint32_t PERIOD) {
    if (pFunction == NULL) return NO_TASK_ID;

    __disable_irq();

    // Cấp phát slot từ Free List trong O(1)
    if (free_head == NULL) {
        Error_code_G = ERROR_SCH_TOO_MANY_TASKS;
        __enable_irq();
        return NO_TASK_ID; // Hàng đợi đã đầy
    }

    sTask* newTask = free_head;
    free_head = free_head->pNext;

    // Quy đổi thời gian từ mili-giây sang ticks (1 tick = 10ms)
    uint32_t scaled_delay = (DELAY == 0) ? 0 : ((DELAY + 9) / 10);
    uint32_t scaled_period = (PERIOD == 0) ? 0 : ((PERIOD + 9) / 10);

    newTask->pTask = pFunction;
    newTask->Delay = scaled_delay;
    newTask->Period = scaled_period;
    newTask->RunMe = 0;
    newTask->pPrev = NULL;
    newTask->pNext = NULL;

    // Chèn vào Delta-Time Queue
    SCH_Insert_To_Queue(newTask);

    uint32_t assignedID = newTask->TaskID;
    __enable_irq();

    return assignedID;
}

/*
 * --- XÓA TÁC VỤ KHỎI BỘ LẬP LỊCH TRONG O(1) ---
 * Sử dụng TaskID để truy cập trực tiếp slot mảng trong O(1).
 * Nhờ liên kết đôi (Doubly-Linked), gỡ node và cộng dồn delta delay trong O(1).
 * Thu hồi slot về Free List trong O(1).
 */
uint8_t SCH_Delete_Task(uint32_t taskID) {
    if (taskID == 0 || taskID > SCH_MAX_TASKS) {
        Error_code_G = ERROR_SCH_CANNOT_DELETE_TASK;
        return RETURN_ERROR;
    }

    __disable_irq();
    sTask* target = &task_pool[taskID - 1];

    if (target->pTask == NULL) {
        Error_code_G = ERROR_SCH_CANNOT_DELETE_TASK;
        __enable_irq();
        return RETURN_ERROR;
    }

    // Nếu target đang nằm trong Delta Queue, thực hiện gỡ liên kết trong O(1)
    if (target == SCH_head_G || target->pPrev != NULL || target->pNext != NULL) {
        if (target == SCH_head_G) {
            SCH_head_G = target->pNext;
            if (SCH_head_G != NULL) {
                SCH_head_G->pPrev = NULL;
                SCH_head_G->Delay += target->Delay; // Bù delta delay cho node kế
            }
        } else {
            if (target->pPrev != NULL) {
                target->pPrev->pNext = target->pNext;
            }
            if (target->pNext != NULL) {
                target->pNext->pPrev = target->pPrev;
                target->pNext->Delay += target->Delay; // Bù delta delay cho node kế
            }
        }
    }

    // Reset dữ liệu task và hoàn trả slot về Free List trong O(1)
    target->pTask = NULL;
    target->Delay = 0;
    target->Period = 0;
    target->RunMe = 0;
    target->pPrev = NULL;
    target->pNext = free_head;
    free_head = target;

    __enable_irq();
    return RETURN_NORMAL;
}
